#include "DeepSeekClient.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "Json.h"
#include "Platform.h"
#include "Util.h"

namespace {

// Private temp file (0600 on POSIX), removed on destruction.
class TempFile {
public:
    explicit TempFile(const std::string& content) : path_(platform::writeTempFile(content)) {}
    ~TempFile() {
        if (!path_.empty()) std::remove(path_.c_str());
    }
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;

    bool ok() const { return !path_.empty(); }
    const std::string& path() const { return path_; }

private:
    std::string path_;
};

std::string curlConfigQuote(const std::string& s) {
    return "\"" + util::replaceAll(util::replaceAll(s, "\\", "\\\\"), "\"", "\\\"") + "\"";
}

}  // namespace

DeepSeekClient::DeepSeekClient(DeepSeekSettings settings) : settings_(std::move(settings)) { loadCredentials(); }

bool DeepSeekClient::keyFromEnvironment() const {
    const char* env = std::getenv("DEEPSEEK_API_KEY");
    return env && *env;
}

void DeepSeekClient::setKeyFile(const std::string& path) {
    settings_.keyFile = path;
    apiKey_.clear();
    credentialsError_.clear();
    loadCredentials();
}

void DeepSeekClient::loadCredentials() {
    if (const char* env = std::getenv("DEEPSEEK_API_KEY"); env && *env) {
        apiKey_ = util::trim(env);
        return;
    }
    if (settings_.keyFile.empty()) {
        credentialsError_ = "no key file configured";
        return;
    }
    std::ifstream in(settings_.keyFile);
    if (!in) {
        credentialsError_ = "key file not found: " + settings_.keyFile;
        return;
    }
    std::string line, firstLine;
    while (std::getline(in, line)) {
        line = util::trim(line);
        if (line.empty() || line[0] == '#') continue;
        if (firstLine.empty()) firstLine = line;
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = util::toLower(util::trim(line.substr(0, eq)));
        const std::string value = util::trim(line.substr(eq + 1));
        if (key == "deepseek.api.key" || key == "api_key" || key == "deepseek_api_key") apiKey_ = value;
        else if (key == "deepseek.api.url" || key == "api_url") settings_.url = value;
        else if (key == "deepseek.api.model" || key == "model") settings_.model = value;
    }
    if (apiKey_.empty() && firstLine.find('=') == std::string::npos) apiKey_ = firstLine;  // raw key file
    if (apiKey_.empty()) credentialsError_ = "no API key in " + settings_.keyFile;
}

std::string DeepSeekClient::statusLine() const {
    if (isConfigured()) return "DeepSeek ready (" + settings_.model + ")";
    return "DeepSeek offline — " + credentialsError_;
}

ApiResult DeepSeekClient::chat(const std::string& systemPrompt, const std::string& userPrompt) const {
    if (!isConfigured()) return {false, {}, credentialsError_};

    json::Value body = json::Value::object();
    body.set("model", settings_.model);
    json::Value messages = json::Value::array();
    for (const auto& [role, content] : {std::pair{"system", systemPrompt}, std::pair{"user", userPrompt}}) {
        json::Value m = json::Value::object();
        m.set("role", role);
        m.set("content", content);
        messages.push_back(std::move(m));
    }
    body.set("messages", std::move(messages));
    body.set("temperature", 0.3);
    body.set("max_tokens", 900);
    body.set("stream", false);

    TempFile bodyFile(body.dump());
    TempFile configFile("url = " + curlConfigQuote(settings_.url) + "\n" +
                        "header = " + curlConfigQuote("Authorization: Bearer " + apiKey_) + "\n" +
                        "header = \"Content-Type: application/json\"\n" +
                        "data-binary = " + curlConfigQuote("@" + bodyFile.path()) + "\n" +
                        "write-out = \"\\n%{http_code}\"\n");
    if (!bodyFile.ok() || !configFile.ok()) return {false, {}, "could not create temporary files"};

    const std::string curl = platform::findCommand("curl");
    if (curl.empty()) return {false, {}, "curl is not installed"};
    const std::string command = platform::quote(curl) + " -sS -X POST --max-time " + std::to_string(settings_.timeoutSec) +
                                " -K " + platform::quote(configFile.path()) + " 2>&1";
    const platform::CommandResult proc = platform::runCapture(command);

    const auto lastNl = proc.output.find_last_of('\n');
    const std::string httpCode = lastNl == std::string::npos ? "" : util::trim(proc.output.substr(lastNl + 1));
    const std::string payload = lastNl == std::string::npos ? proc.output : proc.output.substr(0, lastNl);

    if (proc.exitCode != 0) return {false, {}, "network error: " + util::trim(payload)};

    try {
        const json::Value resp = json::Value::parse(payload);
        if (httpCode != "200") {
            const std::string msg = resp["error"].getString("message", "unexpected response");
            return {false, {}, "HTTP " + httpCode + ": " + msg};
        }
        const auto& choices = resp["choices"].asArray();
        if (choices.empty()) return {false, {}, "empty response from API"};
        const std::string content = choices.front()["message"].getString("content");
        if (content.empty()) return {false, {}, "API returned no content"};
        return {true, content, {}};
    } catch (const json::ParseError&) {
        return {false, {}, "HTTP " + httpCode + ": invalid JSON response"};
    }
}

ApiResult DeepSeekClient::explain(const ExplainRequest& req) const {
    const bool correct = req.userAnswer == req.correctAnswer;
    const std::string structure =
        correct ? "### 1. Why this answer is correct\n### 2. Why the other options are wrong\n### 3. Memory tip / example\n"
                : "### 1. Why your answer is wrong\n### 2. The correct answer and why\n### 3. Memory tip / example\n";
    const std::string system =
        "You are an experienced, friendly " + req.studyLanguage + " teacher preparing a student for the " + req.level +
        " exam. Always answer in " + settings_.language + " (" + req.exampleStyle + "). "
        "Be precise and concise (max ~220 words). Use this structure with '### ' headings:\n" + structure +
        "Translate the headings into the answer language. Use **bold** for key words and '- ' bullets. "
        "No tables, no code blocks.";

    std::string user = req.level + " — section: " + req.section + "\n";
    if (!req.instruction.empty()) user += "Instruction: " + req.instruction + "\n";
    if (!req.passage.empty()) {
        user += (req.section == "listening" ? "Audio transcript:\n" : "Passage:\n") + req.passage + "\n";
    }
    user += "Question: " + req.question + "\n";
    user += "(Text in 【】 is the underlined part.)\nOptions:\n";
    for (std::size_t i = 0; i < req.options.size(); ++i)
        user += "  " + std::string(1, static_cast<char>('A' + i)) + ") " + req.options[i] + "\n";
    if (correct) {
        user += "Correct answer (the student chose it, maybe by guessing): " + req.correctAnswer + "\n";
        user += "Explain why this answer is correct and why each of the other options does not fit.";
    } else {
        user += "Student's answer (wrong): " + req.userAnswer + "\n";
        user += "Correct answer: " + req.correctAnswer + "\n";
        user += "Explain why the student's answer is wrong and why the correct one is right.";
    }
    return chat(system, user);
}

ApiResult DeepSeekClient::translate(const TranslateRequest& req) const {
    const std::string system =
        "You are a " + req.studyLanguage + " teacher helping a student preparing for the " + req.level +
        " exam. Translate " + req.studyLanguage + " text into " + settings_.language +
        ". Give a natural translation, then a short literal gloss of key words if helpful. "
        "Be concise (max ~120 words). Use **bold** for key words and '- ' bullets. No tables, no code blocks.";

    std::string user = req.level + " — section: " + req.section + "\n";
    if (!req.passage.empty()) {
        user += (req.section == "listening" ? "Audio transcript:\n" : "Passage:\n") + req.passage + "\n";
    }
    user += "Sentence: " + req.question + "\n";
    user += "Correct answer for the blank / underlined part (【】): " + req.correctAnswer + "\n";
    user += "Fill in the correct answer and translate the complete sentence";
    user += req.passage.empty() ? "." : ", then the passage/transcript.";
    return chat(system, user);
}
