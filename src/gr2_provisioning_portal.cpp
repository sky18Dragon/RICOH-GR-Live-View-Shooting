#include "gr2_provisioning_portal.h"

#include <WiFi.h>

#include "config.h"
#include "gr2_provisioning_logic.h"

namespace {
constexpr uint16_t kDnsPort = 53;
constexpr uint32_t kResponseFlushMs = 1200;
constexpr int kMaxNetworks = 20;
}  // namespace

bool Gr2ProvisioningPortal::begin() {
  stop();
  _lastError = "";
  _pending = Gr2ProvisionedWifi{};
  _networkOptions = "";
  _submitted = false;
  _submittedAtMs = 0;
  _lastStationCount = 0;

  const uint64_t chipId = ESP.getEfuseMac();
  char suffix[5] = {0};
  snprintf(suffix, sizeof(suffix), "%04X", static_cast<unsigned>(chipId & 0xFFFFU));
  _accessPointSsid = String(GR2_PROVISIONING_AP_PREFIX) + suffix;
  _accessPointPassword = GR2_PROVISIONING_AP_PASSWORD;

  // Espressif documents blocking scans as a station operation. Complete the
  // scan before starting SoftAP so an HTTP request never waits on an
  // all-channel scan and the AP never changes channels underneath the phone.
  WiFi.mode(WIFI_OFF);
  delay(50);
  WiFi.mode(WIFI_STA);
  delay(50);
  WiFi.setSleep(true);
  WiFi.setAutoReconnect(false);
  _networkOptions = scanNetworkOptions();

  WiFi.mode(WIFI_AP);
  delay(50);
  IPAddress portalIp;
  if (!portalIp.fromString(GR2_PROVISIONING_URL_HOST) ||
      !WiFi.softAPConfig(portalIp, portalIp, IPAddress(255, 255, 255, 0))) {
    _lastError = "Could not configure setup AP";
    WiFi.mode(WIFI_STA);
    return false;
  }
  if (!WiFi.softAP(_accessPointSsid.c_str(), _accessPointPassword.c_str())) {
    _lastError = "Could not start setup AP";
    WiFi.mode(WIFI_STA);
    return false;
  }
  delay(100);
  if (WiFi.softAPIP() != portalIp) {
    _lastError = String("Unexpected setup IP: ") + WiFi.softAPIP().toString();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    return false;
  }

  configureRoutes();
  _server.begin();
  if (!_dns.start(kDnsPort, "*", WiFi.softAPIP())) {
    _server.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    _lastError = "Could not start setup DNS";
    return false;
  }

  _active = true;
  Serial.printf("GR II setup: AP='%s' URL=http://%s HTTP=ready\n",
                _accessPointSsid.c_str(),
                WiFi.softAPIP().toString().c_str());
  return true;
}

void Gr2ProvisioningPortal::loop() {
  if (!_active) {
    return;
  }
  _dns.processNextRequest();
  _server.handleClient();
  const uint8_t stationCount = WiFi.softAPgetStationNum();
  if (stationCount != _lastStationCount) {
    _lastStationCount = stationCount;
    Serial.printf("GR II setup: connected phones=%u\n",
                  static_cast<unsigned>(stationCount));
  }
}

void Gr2ProvisioningPortal::stop() {
  if (_active) {
    _dns.stop();
    _server.stop();
    WiFi.softAPdisconnect(true);
  }
  WiFi.scanDelete();
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  _pending = Gr2ProvisionedWifi{};
  _accessPointPassword = "";
  _networkOptions = "";
  _submitted = false;
  _submittedAtMs = 0;
  _active = false;
}

bool Gr2ProvisioningPortal::takeCredentials(Gr2ProvisionedWifi& credentials) {
  if (!gr2CredentialsReadyForHandoff(
          _active, _submitted, _submittedAtMs, millis(), kResponseFlushMs)) {
    return false;
  }
  credentials = _pending;
  _pending = Gr2ProvisionedWifi{};
  _submitted = false;
  return true;
}

String Gr2ProvisioningPortal::accessPointIp() const {
  return _active ? WiFi.softAPIP().toString() : String(GR2_PROVISIONING_URL_HOST);
}

void Gr2ProvisioningPortal::configureRoutes() {
  if (_routesConfigured) {
    return;
  }
  _server.on("/", HTTP_GET, [this]() { handleRoot(); });
  _server.on("/scan", HTTP_GET, [this]() { handleRoot(); });
  _server.on("/health", HTTP_GET, [this]() {
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "text/plain; charset=utf-8", "ok");
  });
  _server.on("/save", HTTP_POST, [this]() { handleSave(); });
  _server.on("/generate_204", HTTP_ANY, [this]() { redirectToRoot(); });
  _server.on("/hotspot-detect.html", HTTP_ANY, [this]() { redirectToRoot(); });
  _server.on("/ncsi.txt", HTTP_ANY, [this]() { redirectToRoot(); });
  _server.on("/connecttest.txt", HTTP_ANY, [this]() { redirectToRoot(); });
  _server.onNotFound([this]() { redirectToRoot(); });
  _routesConfigured = true;
}

void Gr2ProvisioningPortal::handleRoot() {
  String content;
  content.reserve(2100 + _networkOptions.length());
  content += F("<h1>RICOH GR II 配网</h1>");
  content += F("<p>请从启动配网热点前扫描到的网络中选择相机 Wi-Fi。</p>");
  content += F("<form method='post' action='/save'><label>相机 Wi-Fi</label>");
  content += F("<select name='ssid'>");
  content += _networkOptions;
  content += F("</select><label>未找到时手动输入 SSID（可选）</label>");
  content += F("<input name='manual_ssid' maxlength='32' autocomplete='off' placeholder='例如 RICOH_123456'>");
  content += F("<label>Wi-Fi 密码</label>");
  content += F("<input name='password' type='password' minlength='8' maxlength='63' autocomplete='current-password' placeholder='相机显示的密码'>");
  content += F("<button type='submit'>保存并连接相机</button></form>");
  content += F("<p class='hint'>需要重新扫描时，长按 StickS3 的 B 键取消，再重新确认 GR II。</p>");
  content += F("<p class='hint'>提交后手机会与 StickS3 热点断开，这是正常现象。</p>");
  _server.sendHeader("Cache-Control", "no-store");
  _server.send(200, "text/html; charset=utf-8", pageShell(content));
}

void Gr2ProvisioningPortal::handleSave() {
  const Gr2ProvisioningForm form{
      _server.arg("ssid").c_str(),
      _server.arg("manual_ssid").c_str(),
      _server.arg("password").c_str(),
      _accessPointSsid.c_str(),
  };
  const Gr2ProvisioningValidation validation = validateGr2ProvisioningForm(form);
  if (!validation.valid) {
    String content = F("<h1>配置无效</h1><p>请选择相机热点；密码应为空或为 8–63 个字符。</p>");
    content += F("<a class='button' href='/'>返回</a>");
    _server.send(400, "text/html; charset=utf-8", pageShell(content));
    return;
  }

  _pending.ssid = validation.ssid.c_str();
  _pending.passphrase = validation.passphrase.c_str();
  _submitted = true;
  _submittedAtMs = millis();
  _server.sendHeader("Cache-Control", "no-store");
  _server.send(200,
               "text/html; charset=utf-8",
               pageShell(F("<h1>配置已接收</h1><p>StickS3 正在保存并连接相机，请查看设备屏幕。</p>")));
  Serial.printf("GR II setup: credentials submitted for ssid='%s'\n", _pending.ssid.c_str());
}

void Gr2ProvisioningPortal::redirectToRoot() {
  _server.sendHeader("Location", String("http://") + GR2_PROVISIONING_URL_HOST + "/", true);
  _server.send(302, "text/plain", "");
}

String Gr2ProvisioningPortal::scanNetworkOptions() {
  const int count = WiFi.scanNetworks(false, true, false, 300U);
  if (count <= 0) {
    Serial.printf("GR II setup: pre-AP scan returned %d networks\n", count);
    WiFi.scanDelete();
    return renderGr2NetworkOptions({}, _accessPointSsid.c_str(), false).html.c_str();
  }

  const int limit = count < kMaxNetworks ? count : kMaxNetworks;
  std::vector<Gr2ScannedNetwork> networks;
  networks.reserve(limit);
  for (int i = 0; i < limit; ++i) {
    networks.push_back({WiFi.SSID(i).c_str(), WiFi.RSSI(i)});
  }
  WiFi.scanDelete();
  const Gr2NetworkOptions options =
      renderGr2NetworkOptions(networks, _accessPointSsid.c_str(), true);
  Serial.printf("GR II setup: pre-AP scan found %d networks, ricoh=%d\n",
                count,
                options.ricohFound ? 1 : 0);
  return options.html.c_str();
}

String Gr2ProvisioningPortal::pageShell(const String& content) const {
  String page;
  page.reserve(2300 + content.length());
  page += F("<!doctype html><html lang='zh-CN'><head><meta charset='utf-8'>");
  page += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
  page += F("<title>GR II 配网</title><style>");
  page += F("body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background:#111;color:#f5f5f5;margin:0;padding:24px}");
  page += F("main{max-width:520px;margin:24px auto;background:#1d1d1f;padding:24px;border-radius:18px;box-shadow:0 12px 36px #0008}");
  page += F("h1{font-size:25px;margin:0 0 12px}p{line-height:1.6;color:#c7c7cc}label{display:block;margin:18px 0 7px;font-weight:600}");
  page += F("select,input,button,.button{box-sizing:border-box;width:100%;font-size:17px;border-radius:12px;padding:13px;border:1px solid #48484a}");
  page += F("select,input{background:#2c2c2e;color:#fff}button,.button{display:block;margin-top:22px;background:#31d158;color:#071b0b;border:0;font-weight:700;text-align:center;text-decoration:none}");
  page += F(".refresh{display:block;text-align:center;color:#63a9ff;margin-top:20px}.hint{font-size:14px;color:#8e8e93}</style></head><body><main>");
  page += content;
  page += F("</main></body></html>");
  return page;
}
