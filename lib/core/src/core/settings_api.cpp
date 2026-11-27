#include "core/settings_api.h"

#include <vector>

namespace core {

namespace {

constexpr char kTempPath[] = "/config.json.tmp";

SettingsSaveResult failure(int status, const std::string& error) {
  SettingsSaveResult r;
  r.status = status;
  JsonDocument doc;
  doc["saved"] = false;
  doc["error"] = error;
  doc["issues"].to<JsonArray>();
  serializeJson(doc, r.json);
  return r;
}

bool writeConfig(ConfigStore& store, const std::string& text) {
  return store.write(kTempPath, text) && store.rename(kTempPath, kConfigPath);
}

struct SecretChange {
  std::string name;
  bool erase = false;
  std::string value;
};

}  // namespace

SettingsSaveResult saveSettings(const std::string& body, ConfigStore& store,
                                const ConfigValidator& validate, SecretStore* secrets) {
  JsonDocument request;
  if (body.size() > 2 * kMaxConfigBytes || deserializeJson(request, body) ||
      !request.is<JsonObject>()) {
    return failure(400, "invalid request");
  }
  JsonObjectConst config = request["config"];
  if (config.isNull()) return failure(400, "config missing");

  std::string text;
  serializeJsonPretty(config, text);
  text += "\n";
  ConfigIssues issues;
  JsonDocument parsed;
  if (!readConfig(text.data(), text.size(), parsed, issues)) {
    return failure(400, issues.empty() ? "invalid configuration" : issues.front().message);
  }
  if (validate) validate(parsed.as<JsonObjectConst>(), issues);

  // Secrets: checked first, a bad value is never saved.
  ConfigIssues secret_issues;
  std::vector<SecretChange> changes;
  for (JsonPairConst p : request["secrets"].as<JsonObjectConst>()) {
    const std::string name = p.key().c_str();
    const std::string path = "secrets." + name;
    if (!isKnownSecret(name)) {
      secret_issues.push_back({path, "unknown secret"});
      continue;
    }
    SecretChange change;
    change.name = name;
    if (p.value().isNull()) {
      change.erase = true;
    } else {
      change.value = p.value().as<std::string>();
      std::string reason;
      if (!p.value().is<const char*>() || !checkSecret(name, change.value, reason)) {
        secret_issues.push_back({path, reason.empty() ? "must be text" : reason});
        continue;
      }
    }
    changes.push_back(change);
  }
  if (!changes.empty() && secrets == nullptr) {
    secret_issues.push_back({"secrets", "secrets not available"});
  }

  const bool force = request["force"] | false;
  SettingsSaveResult r;
  JsonDocument reply;
  JsonArray list = reply["issues"].to<JsonArray>();
  for (const ConfigIssues* group : {&secret_issues, &issues}) {
    for (const ConfigIssue& i : *group) {
      JsonObject o = list.add<JsonObject>();
      o["path"] = i.path;
      o["message"] = i.message;
      // Problems of secrets cannot be overridden with "force".
      o["blocking"] = group == &secret_issues;
    }
  }
  if (!secret_issues.empty() || (!issues.empty() && !force)) {
    reply["saved"] = false;
    r.status = 422;
    serializeJson(reply, r.json);
    return r;
  }
  // Secrets first: if one cannot be saved, the old file stays as it was.
  for (const SecretChange& c : changes) {
    const bool ok =
        c.erase ? secrets->erase(c.name.c_str()) : secrets->write(c.name.c_str(), c.value);
    if (!ok) return failure(500, "cannot save " + c.name);
  }
  // A calendar address in the file itself (e.g. an imported one) is dropped;
  // the check above has reported it.
  if (stripSecrets(parsed.as<JsonObject>())) {
    text.clear();
    serializeJsonPretty(parsed, text);
    text += "\n";
  }
  if (!writeConfig(store, text)) return failure(500, "cannot write the file");
  reply["saved"] = true;
  serializeJson(reply, r.json);
  r.restart = true;
  return r;
}

SettingsSaveResult resetSettings(ConfigStore& store, SecretStore& secrets) {
  if (!secrets.erase(kSecretCalendarUrl)) return failure(500, "cannot clear the calendar address");
  if (!writeConfig(store, "{}\n")) return failure(500, "cannot write the file");
  SettingsSaveResult r;
  r.json = R"({"saved":true,"issues":[]})";
  r.restart = true;
  return r;
}

std::string storedConfig(ConfigStore& store, std::string* problem) {
  std::string text;
  if (!store.read(kConfigPath, text)) {
    if (problem != nullptr) *problem = "no configuration file";
    return "{}";
  }
  JsonDocument doc;
  ConfigIssues issues;
  if (!readConfig(text.data(), text.size(), doc, issues)) {
    if (problem != nullptr) *problem = issues.empty() ? "invalid file" : issues.front().message;
    return "{}";
  }
  if (stripSecrets(doc.as<JsonObject>())) {  // written by an older firmware
    text.clear();
    serializeJsonPretty(doc, text);
    text += "\n";
  }
  return text;
}

}  // namespace core
