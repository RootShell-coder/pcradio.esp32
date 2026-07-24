#include "wrapper.h"
#include "esp_log.h"
#include <string.h>

#include "codec_mp3.h"
#include "codec_aac.h"

static const char *TAG = "WRAPPER";

esp_err_t wrapper_open_detected_decoder(wrapper_audio_type_t type,
                                        esp_audio_dec_handle_t *handle_out)
{
    if (!handle_out) {
        return ESP_ERR_INVALID_ARG;
    }

    *handle_out = NULL;

    ESP_LOGI(TAG, "Opening decoder for type %d", (int)type);

    switch (type) {

        case WRAPPER_TYPE_MP3:
            return codec_open_mp3_decoder(handle_out);

        case WRAPPER_TYPE_AAC:
            return codec_open_aac_decoder(handle_out);

        default:
            ESP_LOGE(TAG, "Unsupported decoder type %d", (int)type);
            return ESP_ERR_NOT_SUPPORTED;
    }
}

wrapper_audio_type_t wrapper_detect_audio_type_from_data(const uint8_t *data,
                                                         size_t len)
{
    if (!data || len < 2) {
        return WRAPPER_TYPE_NONE;
    }

    /*
     * Не предполагаем, что поток начинается с начала MP3/AAC кадра.
     * Ищем sync word внутри всего полученного буфера.
     */
    for (size_t i = 0; i < len - 1; i++) {

        if (data[i] != 0xFF) {
            continue;
        }

        uint8_t second = data[i + 1];

        /* ================= AAC ADTS ================= */

        if (second == 0xF1 || second == 0xF9) {

            ESP_LOGI(TAG,
                     "Detected AAC at offset %u (%02X %02X)",
                     (unsigned)i,
                     data[i],
                     second);

            return WRAPPER_TYPE_AAC;
        }

        /* ================= MP3 ================= */

        /*
         * MPEG sync (11 единиц)
         */
        if ((second & 0xE0) != 0xE0) {
            continue;
        }

        /*
         * Layer III
         */
        if ((second & 0x06) != 0x02) {
            continue;
        }

        /*
         * Проверяем bitrate index.
         * 0000 и 1111 запрещены.
         */
        uint8_t bitrate = (data[i + 2] >> 4) & 0x0F;

        if (bitrate == 0 || bitrate == 0x0F) {
            continue;
        }

        /*
         * Проверяем sample rate.
         * 11 запрещено.
         */
        uint8_t samplerate = (data[i + 2] >> 2) & 0x03;

        if (samplerate == 0x03) {
            continue;
        }

        ESP_LOGI(TAG,
                 "Detected MP3 at offset %u (%02X %02X %02X %02X)",
                 (unsigned)i,
                 data[i],
                 data[i + 1],
                 (i + 2 < len) ? data[i + 2] : 0,
                 (i + 3 < len) ? data[i + 3] : 0);

        return WRAPPER_TYPE_MP3;
    }

    ESP_LOGW(TAG,
             "No MP3/AAC sync found in %u bytes",
             (unsigned)len);

    return WRAPPER_TYPE_NONE;
}

wrapper_audio_type_t wrapper_detect_audio_type_from_url(const char *url)
{
    if (!url) {
        return WRAPPER_TYPE_NONE;
    }

    const char *extension = strrchr(url, '.');

    if (extension) {

        extension++;

        if (!strcasecmp(extension, "mp3")) {
            return WRAPPER_TYPE_MP3;
        }

        if (!strcasecmp(extension, "aac")) {
            return WRAPPER_TYPE_AAC;
        }
    }

    if (strstr(url, "mp3")) {
        return WRAPPER_TYPE_MP3;
    }

    if (strstr(url, "aac")) {
        return WRAPPER_TYPE_AAC;
    }

    ESP_LOGW(TAG, "Cannot determine audio type from URL: %s", url);

    return WRAPPER_TYPE_NONE;
}

void wrapper_dec_close(esp_audio_dec_handle_t handle)
{
    if (handle) {
        esp_audio_dec_close(handle);
    }
}
