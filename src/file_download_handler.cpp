#include "app.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>


// --- Function to display download progress ---
void showDownloadProgress(int currentFile, int percent) {
    showFileDownloadProgress(currentFile, percent); 
    char buf[40]; 
    snprintf(buf, sizeof(buf), "File %d/%d", currentFile);
    Serial.printf("[Progress] %s: %d%%\n", buf, percent);
}

// --- Definitions ---

#define URL_QUEUE_LENGTH        5
#define URL_MAX_LENGTH          2048 
#define CHUNK_SIZE              1024 // Size of each data chunk read from HTTP stream
#define CHUNK_QUEUE_LENGTH      2    // Number of chunks that can be buffered between download and write tasks

// --- Queues and Task Handles ---
static QueueHandle_t urlQueue = NULL;
static TaskHandle_t downloadTaskHandle = NULL;
static TaskHandle_t writeTaskHandle = NULL;

struct FileChunk {
    uint8_t data[CHUNK_SIZE];
    size_t length;
    bool isLast;        // True if this is the last marker chunk for a file
    char filename[64];  // Set in the first data chunk of a file, or in the 'isLast' marker for 0-byte files
};
static QueueHandle_t chunkQueue = NULL;

// --- Forward Declarations ---
void downloadTask(void* pvParameters);
void writeTask(void* pvParameters);

// --- Initialization ---
void initFileDownloadHandler() {
    size_t freeHeap = ESP.getFreeHeap();
    size_t usableHeapForTasks = freeHeap / 2; // Reserve half for system, wifi, ssl
    size_t dynamicStackSize = usableHeapForTasks / 2; // Split remaining between the two tasks

    // Clamp stack size to reasonable limits
    if (dynamicStackSize < 6144) dynamicStackSize = 6144; // Minimum for HTTPClient with SSL
    if (dynamicStackSize > 16384) dynamicStackSize = 16384;

    Serial.printf("[FileHandler] Initializing. Free heap: %u, Calculated stack per task: %u\n",
                  (unsigned int)freeHeap, (unsigned int)dynamicStackSize);

    if (!urlQueue) {
        urlQueue = xQueueCreate(URL_QUEUE_LENGTH, URL_MAX_LENGTH);
        if (!urlQueue) Serial.println("[FileHandler] ERROR: Failed to create urlQueue!");
    }
    if (!chunkQueue) {
        chunkQueue = xQueueCreate(CHUNK_QUEUE_LENGTH, sizeof(FileChunk));
        if (!chunkQueue) Serial.println("[FileHandler] ERROR: Failed to create chunkQueue!");
    }

    if (!downloadTaskHandle && urlQueue && chunkQueue) { // Only create task if queues are up
        xTaskCreatePinnedToCore(downloadTask, "DownloadTask", dynamicStackSize, NULL, 2, &downloadTaskHandle, 0); // Core 0 for Network
    }
    if (!writeTaskHandle && chunkQueue) { // Only create task if its queue is up
        xTaskCreatePinnedToCore(writeTask, "WriteTask", dynamicStackSize, NULL, 2, &writeTaskHandle, 1);    // Core 1 for SD
    }
}

// --- Enqueue URL for Download ---
void enqueueDownloadUrl(const char* url, const char* s3Key) { // Pass s3Key for filename
    if (urlQueue && url && s3Key) {
        char urlToQueue[URL_MAX_LENGTH];
        strncpy(urlToQueue, url, URL_MAX_LENGTH - 1);
        urlToQueue[URL_MAX_LENGTH - 1] = '\0';

        if (xQueueSend(urlQueue, urlToQueue, pdMS_TO_TICKS(100)) != pdPASS) {
            Serial.println("[FileHandler] Failed to enqueue URL, queue full?");
        } else {
            Serial.printf("[FileHandler] Enqueued URL for key: %s\n", s3Key);
        }
    } else {
        Serial.println("[FileHandler] Cannot enqueue URL: Queue not init or URL/key is null.");
    }
}


// --- Download Task (Core 0) ---
void downloadTask(void* pvParameters) {
    Serial.println("[DownloadTask] Started.");
    while (!urlQueue || !chunkQueue) {
        Serial.println("[DownloadTask] Waiting for queues to be initialized...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    char currentPresignedUrl[URL_MAX_LENGTH];
    // TODO: fileCounter and totalFilesForBatch should ideally be managed based on
    // the number of URLs received in one MQTT message batch.
    int fileDownloadAttemptCounter = 0; 

    for (;;) {
        if (xQueueReceive(urlQueue, currentPresignedUrl, portMAX_DELAY) == pdTRUE) {
            fileDownloadAttemptCounter++;
            Serial.printf("[DownloadTask] Processing URL #%d: %s\n", fileDownloadAttemptCounter, currentPresignedUrl);

            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("[DownloadTask] WiFi not connected! Skipping download.");
                // Send a dummy "last chunk" marker so writeTask doesn't stall if it expects one for this URL
                FileChunk errorMarker;
                errorMarker.isLast = true;
                errorMarker.length = 0;
                errorMarker.filename[0] = '\0';
                xQueueSend(chunkQueue, &errorMarker, pdMS_TO_TICKS(100));
                continue;
            }

            // Extract filename from the S3 key part of the URL
            char extractedFilename[sizeof(FileChunk::filename)] = "unknown.dat"; // Default
            const char* s3KeyStart = strstr(currentPresignedUrl, ".com/"); // Find end of domain
            if (s3KeyStart) {
                s3KeyStart += strlen(".com/"); // Move past ".com/"
                const char* queryParams = strchr(s3KeyStart, '?');
                int keyLength = 0;
                if (queryParams) {
                    keyLength = queryParams - s3KeyStart;
                } else {
                    keyLength = strlen(s3KeyStart);
                }

                if (keyLength > 0) {
                    const char* lastSlashInKey = NULL;
                    for (int i = keyLength - 1; i >= 0; i--) {
                        if (s3KeyStart[i] == '/') {
                            lastSlashInKey = &s3KeyStart[i];
                            break;
                        }
                    }
                    const char* namePart = lastSlashInKey ? lastSlashInKey + 1 : s3KeyStart;
                    int namePartLength = keyLength - (namePart - s3KeyStart);

                    if (namePartLength >= sizeof(extractedFilename)) {
                        namePartLength = sizeof(extractedFilename) - 1;
                    }
                    strncpy(extractedFilename, namePart, namePartLength);
                    extractedFilename[namePartLength] = '\0';
                }
            }
            Serial.printf("[DownloadTask] Target filename: %s\n", extractedFilename);

            HTTPClient http;
            WiFiClientSecure clientSecure; // Use a new client for each request for safety

            // --- CRITICAL: SET THE ROOT CA CERTIFICATE ---
            clientSecure.setCACert(AWS_CERT_CA); 

            bool downloadSuccessful = false;
            int totalBytesExpected = -1;
            int bytesDownloadedThisFile = 0;
            bool firstDataChunkSent = false;

            Serial.printf("[DownloadTask] HTTP Begin for: %s\n", extractedFilename);
            if (http.begin(clientSecure, currentPresignedUrl)) {
                Serial.printf("[DownloadTask] HTTP GET for: %s\n", extractedFilename);
                int httpCode = http.GET();

                if (httpCode > 0) { // Positive code means server responded
                    if (httpCode == HTTP_CODE_OK) {
                        downloadSuccessful = true; // Tentatively
                        WiFiClient* stream = http.getStreamPtr();
                        totalBytesExpected = http.getSize();
                        Serial.printf("[DownloadTask] File size: %d bytes for %s\n", totalBytesExpected, extractedFilename);

                        if (totalBytesExpected == 0) { // Handle 0-byte files explicitly
                             showDownloadProgress(fileDownloadAttemptCounter, 100);
                        }

                        while (http.connected() && (totalBytesExpected == -1 || bytesDownloadedThisFile < totalBytesExpected || totalBytesExpected == 0)) {
                            if (stream->available()) {
                                FileChunk chunk;
                                chunk.length = stream->read(chunk.data, CHUNK_SIZE);

                                if (chunk.length > 0) {
                                    bytesDownloadedThisFile += chunk.length;
                                    chunk.isLast = false; // This is a data chunk

                                    if (!firstDataChunkSent) {
                                        strncpy(chunk.filename, extractedFilename, sizeof(chunk.filename) - 1);
                                        chunk.filename[sizeof(chunk.filename) - 1] = '\0';
                                        firstDataChunkSent = true;
                                    } else {
                                        chunk.filename[0] = '\0';
                                    }

                                    if (xQueueSend(chunkQueue, &chunk, pdMS_TO_TICKS(5000)) != pdPASS) {
                                        Serial.printf("[DownloadTask] Failed to send data chunk for %s! Aborting file.\n", extractedFilename);
                                        downloadSuccessful = false;
                                        break;
                                    }
                                    int percent = 0;
                                    if (totalBytesExpected > 0) percent = (bytesDownloadedThisFile * 100) / totalBytesExpected;
                                    else if (totalBytesExpected == 0) percent = 100; 
                                    showDownloadProgress(fileDownloadAttemptCounter, percent);

                                } else if (chunk.length < 0) { // Error on read
                                    Serial.printf("[DownloadTask] Stream read error for %s.\n", extractedFilename);
                                    downloadSuccessful = false;
                                    break;
                                }
                            } else if (bytesDownloadedThisFile >= totalBytesExpected && totalBytesExpected != -1) {
                                // All expected bytes read
                                break;
                            } else {
                                vTaskDelay(pdMS_TO_TICKS(10)); // Yield
                            }
                             if (!http.connected() && (totalBytesExpected == -1 || bytesDownloadedThisFile < totalBytesExpected)) {
                                Serial.printf("[DownloadTask] HTTP disconnected prematurely for %s.\n", extractedFilename);
                                downloadSuccessful = false;
                                break;
                            }
                        } // while http.connected
                        if (bytesDownloadedThisFile != totalBytesExpected && totalBytesExpected > 0) {
                            Serial.printf("[DownloadTask] WARN: Bytes downloaded (%d) != total expected (%d) for %s\n",
                                          bytesDownloadedThisFile, totalBytesExpected, extractedFilename);
                        }

                    } else { // HTTP code not OK
                        Serial.printf("[DownloadTask] HTTP GET failed for %s, Code: %d\n", extractedFilename, httpCode);
                        String errorPayload = http.getString(); // Get error body if any
                        Serial.printf("[DownloadTask] HTTP Error Body: %s\n", errorPayload.c_str());
                    }
                } else { // http.GET() returned error (e.g., -1 to -11)
                    Serial.printf("[DownloadTask] HTTP GET failed for %s, Client Error: %d (%s)\n",
                                  extractedFilename, httpCode, http.errorToString(httpCode).c_str());
                }
                http.end();
                Serial.printf("[DownloadTask] End URL processing for %s. Free Heap: %u, Min Free Heap: %u\n",
                    extractedFilename,
                    (unsigned int)ESP.getFreeHeap(),
                    (unsigned int)ESP.getMinFreeHeap());
            } else {
                Serial.printf("[DownloadTask] HTTP Begin FAILED for %s.\n", extractedFilename);
            }

            // Always send a final "isLast" marker chunk for THIS FILE to the write task
            FileChunk lastMarker;
            lastMarker.length = 0;
            lastMarker.isLast = true;
            // If it was a 0-byte file and no data chunks were sent, set filename in marker
            if (downloadSuccessful && totalBytesExpected == 0 && !firstDataChunkSent) {
                strncpy(lastMarker.filename, extractedFilename, sizeof(lastMarker.filename) - 1);
                lastMarker.filename[sizeof(lastMarker.filename) - 1] = '\0';
            } else {
                lastMarker.filename[0] = '\0';
            }

            if (xQueueSend(chunkQueue, &lastMarker, pdMS_TO_TICKS(5000)) != pdPASS) {
                Serial.printf("[DownloadTask] CRITICAL: Failed to send LAST CHUNK marker for %s!\n", extractedFilename);
            } else {
                Serial.printf("[DownloadTask] Sent LAST CHUNK marker for %s.\n", extractedFilename);
            }

            if (downloadSuccessful) {
                Serial.printf("[DownloadTask] Successfully processed download for %s.\n", extractedFilename);
            } else {
                Serial.printf("[DownloadTask] FAILED to download %s.\n", extractedFilename);
            }
        } // if xQueueReceive
        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay if queue is empty
    } // for(;;)
}


// --- Write Task (Core 1) ---
void writeTask(void* pvParameters) {
    Serial.println("[WriteTask] Started.");
    while (!chunkQueue) {
        Serial.println("[WriteTask] Waiting for chunkQueue to be initialized...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    const char* baseWavDirectory = "/ROLAND/SP-404SX/SMPL"; // Base directory
    File currentOutFile;
    char currentFilePath[128] = {0};
    bool isFileOpen = false;
    unsigned long totalBytesWrittenForCurrentFile = 0;

    for (;;) {
        FileChunk chunk;
        if (xQueueReceive(chunkQueue, &chunk, portMAX_DELAY) == pdTRUE) {
            if (!isFileOpen && chunk.filename[0] != '\0' && !chunk.isLast) {
                // This is the first data chunk for a new file
                if (SD.cardType() == CARD_NONE) {
                    Serial.println("[WriteTask] SD card not present! Attempting to re-init SD...");
                    if (!SD.begin(/* pass CS pin if not default */)) { // Attempt to re-initialize
                         Serial.println("[WriteTask] SD.begin() failed on re-attempt.");
                    }
                    vTaskDelay(pdMS_TO_TICKS(500)); 
                    if (SD.cardType() == CARD_NONE) {
                        Serial.printf("[WriteTask] SD still not present. Skipping file: %s\n", chunk.filename);
                        // Drain any subsequent chunks for this phantom file until its 'isLast' marker
                        while (!chunk.isLast) { if (xQueueReceive(chunkQueue, &chunk, pdMS_TO_TICKS(100)) != pdTRUE) break; }
                        continue;
                    }
                }
                // Construct full path: ensure baseWavDirectory exists or create it
                snprintf(currentFilePath, sizeof(currentFilePath), "%s/%s", baseWavDirectory, chunk.filename);


                currentOutFile = SD.open(currentFilePath, FILE_WRITE);
                if (!currentOutFile) {
                    Serial.printf("[WriteTask] Failed to open %s for writing!\n", currentFilePath);
                    // Drain subsequent chunks for this file
                    while (!chunk.isLast) { if (xQueueReceive(chunkQueue, &chunk, pdMS_TO_TICKS(100)) != pdTRUE) break; }
                    continue;
                }
                isFileOpen = true;
                totalBytesWrittenForCurrentFile = 0;
                Serial.printf("[WriteTask] Opened %s for writing.\n", currentFilePath);
            }

            if (isFileOpen && currentOutFile) {
                if (chunk.length > 0) { // It's a data chunk
                    size_t bytesActuallyWritten = currentOutFile.write(chunk.data, chunk.length);
                    if (bytesActuallyWritten != chunk.length) {
                        Serial.printf("[WriteTask] Write error to %s! Wrote %u/%u bytes.\n",
                                      currentFilePath, (unsigned int)bytesActuallyWritten, (unsigned int)chunk.length);
                        currentOutFile.close();
                        isFileOpen = false;
                        // Drain subsequent chunks for this failed file
                        while (!chunk.isLast) { if (xQueueReceive(chunkQueue, &chunk, pdMS_TO_TICKS(100)) != pdTRUE) break; }
                        continue;
                    }
                    totalBytesWrittenForCurrentFile += bytesActuallyWritten;
                    // Serial.printf("[WriteTask] Wrote %u bytes to %s.\n", (unsigned int)bytesActuallyWritten, currentFilePath);
                }

                if (chunk.isLast) { // This is the 'isLast' marker for the current file
                    currentOutFile.close();
                    isFileOpen = false;
                    Serial.printf("[WriteTask] File closed: %s. Total bytes written: %lu\n",
                                  currentFilePath, totalBytesWrittenForCurrentFile);
                    currentFilePath[0] = '\0'; // Clear path for next file
                }
            } else if (chunk.isLast && chunk.filename[0] != '\0') {
                // This is an 'isLast' marker for a 0-byte file (filename is set, length is 0)
                 if (SD.cardType() == CARD_NONE) {
                    Serial.printf("[WriteTask] SD not present, cannot create 0-byte file: %s\n", chunk.filename);
                    continue;
                 }
                snprintf(currentFilePath, sizeof(currentFilePath), "%s/%s", baseWavDirectory, chunk.filename);
                currentOutFile = SD.open(currentFilePath, FILE_WRITE); // Opens for write, creates if not exists, truncates if exists
                if (currentOutFile) {
                    currentOutFile.close(); // Immediately close to create/truncate
                    Serial.printf("[WriteTask] Created/truncated 0-byte file: %s\n", currentFilePath);
                } else {
                    Serial.printf("[WriteTask] Failed to create 0-byte file: %s\n", currentFilePath);
                }
                currentFilePath[0] = '\0';
            } else if (chunk.isLast) {
                // Received an 'isLast' marker but no file was open 
                Serial.println("[WriteTask] Received 'isLast' marker, but no file was open or being processed.");
            } else if (!isFileOpen && chunk.length > 0) {
                Serial.printf("[WriteTask] Received data chunk for '%s' but no file is open. Discarding.\n", chunk.filename);
                 while (!chunk.isLast) { // Drain the rest of this file's chunks
                    if (xQueueReceive(chunkQueue, &chunk, pdMS_TO_TICKS(100)) != pdTRUE) break;
                }
            }
        } 
    } 
}