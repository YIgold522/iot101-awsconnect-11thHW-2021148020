#include <Arduino.h>
#include <IO7F32.h>

String user_html = "";
char* ssid_pfix = (char*)"IOTButton";
unsigned long lastPublishMillis = -pubInterval;

const int Switch = 43;
volatile bool buttonPressed = false;
volatile long lastPressed = 0;

bool switchState = false;  // 토글 상태 저장 (false: off, true: on)

void IRAM_ATTR buttonISR() {
    long now = millis();
    if (now - lastPressed > 200) {  // 디바운싱
        lastPressed = now;
        buttonPressed = true;
    }
}

void publishData() {
    StaticJsonDocument<512> root;
    JsonObject data = root.createNestedObject("d");

    data["switch"] = switchState ? "on" : "off";

    serializeJson(root, msgBuffer);
    client.publish(evtTopic, msgBuffer);
    Serial.println("MQTT 전송: " + String(msgBuffer));
}

void handleUserCommand(char* topic, JsonDocument* root) {
    JsonObject d = (*root)["d"];

    Serial.println(topic);
    if (d.containsKey("switch")) {
        const char* cmd = d["switch"];
        Serial.printf("수신된 switch 명령: %s\n", cmd);

        if (strcmp(cmd, "on") == 0) switchState = true;
        else if (strcmp(cmd, "off") == 0) switchState = false;

        publishData();
        lastPublishMillis = -pubInterval;
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(Switch, INPUT_PULLUP);  // 풀업 설정

    attachInterrupt(digitalPinToInterrupt(Switch), buttonISR, FALLING);

    initDevice();
    JsonObject meta = cfg["meta"];
    pubInterval = meta.containsKey("pubInterval") ? meta["pubInterval"] : 0;
    lastPublishMillis = -pubInterval;

    WiFi.mode(WIFI_STA);
    WiFi.begin((const char*)cfg["ssid"], (const char*)cfg["w_pw"]);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.printf("\nIP address : ");
    Serial.println(WiFi.localIP());

    userCommand = handleUserCommand;
    set_iot_server();
    iot_connect();
}

void loop() {
    if (!client.connected()) {
        iot_connect();
    }
    client.loop();

    if ((pubInterval != 0) && (millis() - lastPublishMillis > pubInterval)) {
        publishData();
        lastPublishMillis = millis();
    }

    if (buttonPressed) {
        buttonPressed = false;
        switchState = !switchState;  // 토글 상태 변경
        publishData();
    }
}