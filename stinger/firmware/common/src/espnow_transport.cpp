#ifdef ARDUINO_ARCH_ESP32

#include "espnow_transport.h"

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

namespace stinger {

EspNowTransport *EspNowTransport::instance_ = nullptr;

namespace {

// ESP-IDF 5 passes RSSI with each received frame; ESP-IDF 4 (Arduino core 2.x) does not, so
// RSSI is reported as 0 ("unknown", PROTOCOL.md section 4.1) there.
#if ESP_IDF_VERSION_MAJOR >= 5
void recvTrampoline(const esp_now_recv_info_t *info, const uint8_t *data, int len);
void sentTrampoline(const uint8_t *mac, esp_now_send_status_t status);
#else
void recvTrampoline(const uint8_t *mac, const uint8_t *data, int len);
void sentTrampoline(const uint8_t *mac, esp_now_send_status_t status);
#endif

} // namespace

EspNowTransport::EspNowTransport(const EspNowConfig &config) : config_(config) {}

bool EspNowTransport::begin() {
    instance_ = this;
    rxQueue_ = xQueueCreate(config_.rxQueueLength, sizeof(RxItem));
    ackDone_ = xSemaphoreCreateBinary();
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_channel(config_.channel, WIFI_SECOND_CHAN_NONE);
    esp_wifi_config_espnow_rate(WIFI_IF_STA, config_.phyRate);
    WiFi.macAddress(ownMac_);
    if (rxQueue_ == nullptr || ackDone_ == nullptr || esp_now_init() != ESP_OK) {
        return false;
    }
    esp_now_register_recv_cb(recvTrampoline);
    esp_now_register_send_cb(sentTrampoline);
    return ensurePeer(Address::broadcast());
}

bool EspNowTransport::ensurePeer(const Address &to) {
    if (esp_now_is_peer_exist(to.mac)) {
        return true;
    }
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, to.mac, sizeof(peer.peer_addr));
    peer.channel = config_.channel;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    return esp_now_add_peer(&peer) == ESP_OK;
}

bool EspNowTransport::send(const Address &to, const uint8_t *packet, size_t len) {
    if (!ensurePeer(to)) {
        return false;
    }
    if (to.isBroadcast()) {
        return esp_now_send(to.mac, packet, len) == ESP_OK;
    }
    // Unicast: wait for the MAC-layer acknowledgement reported by the send callback.
    awaitingAck_ = to;
    delivered_ = false;
    xSemaphoreTake(ackDone_, 0); // clear a stale signal
    waiting_ = true;
    bool ok = esp_now_send(to.mac, packet, len) == ESP_OK &&
              xSemaphoreTake(ackDone_, pdMS_TO_TICKS(config_.ackTimeoutMs)) == pdTRUE && delivered_;
    waiting_ = false;
    return ok;
}

void EspNowTransport::onReceive(ReceiveHandler handler, void *ctx) {
    handler_ = handler;
    ctx_ = ctx;
}

void EspNowTransport::poll() {
    RxItem item;
    while (xQueueReceive(rxQueue_, &item, 0) == pdTRUE) {
        counters_.rxPackets++;
        if (handler_) {
            handler_(item.data, item.len, RxInfo{Address::of(item.mac), item.rssi}, ctx_);
        }
    }
}

// Runs in the Wi-Fi task: copy and hand over, nothing else.
void EspNowTransport::onRecvStatic(const uint8_t *mac, int8_t rssi, const uint8_t *data, int len) {
    EspNowTransport *self = instance_;
    if (self == nullptr || len <= 0 || len > STINGER_MAX_PACKET) {
        return;
    }
    RxItem item;
    memcpy(item.mac, mac, sizeof(item.mac));
    item.rssi = rssi;
    item.len = static_cast<uint8_t>(len);
    memcpy(item.data, data, static_cast<size_t>(len));
    if (xQueueSend(self->rxQueue_, &item, 0) != pdTRUE) {
        self->counters_.rxErrors++; // queue full: poll() is not keeping up
    }
}

void EspNowTransport::onSentStatic(const uint8_t *mac, bool delivered) {
    EspNowTransport *self = instance_;
    if (self == nullptr || !self->waiting_ || !(Address::of(mac) == self->awaitingAck_)) {
        return; // a broadcast, or a send nobody is waiting for
    }
    self->delivered_ = delivered;
    xSemaphoreGive(self->ackDone_);
}

namespace {

#if ESP_IDF_VERSION_MAJOR >= 5
void recvTrampoline(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    int8_t rssi = info->rx_ctrl ? static_cast<int8_t>(info->rx_ctrl->rssi) : 0;
    EspNowTransport::onRecvStatic(info->src_addr, rssi, data, len);
}
#else
void recvTrampoline(const uint8_t *mac, const uint8_t *data, int len) {
    EspNowTransport::onRecvStatic(mac, 0, data, len);
}
#endif

void sentTrampoline(const uint8_t *mac, esp_now_send_status_t status) {
    EspNowTransport::onSentStatic(mac, status == ESP_NOW_SEND_SUCCESS);
}

} // namespace

} // namespace stinger

#endif // ARDUINO_ARCH_ESP32
