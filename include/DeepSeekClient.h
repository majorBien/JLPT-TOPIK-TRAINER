// DeepSeek chat-completions client used by the "why" command.
//
// Transport: the system `curl` binary (platform::runCapture). The API key never appears
// on the command line (ps-visible): it is written to a 0600 temporary curl
// config file (-K) that is deleted right after the request.
//
// Credentials: $DEEPSEEK_API_KEY, or a key file in properties format:
//   deepseek.api.key=sk-...
//   deepseek.api.url=https://api.deepseek.com/v1/chat/completions   (optional)
//   deepseek.api.model=deepseek-chat                                 (optional)
// A file containing only the raw key is accepted as well.
#pragma once

#include <string>
#include <vector>

struct DeepSeekSettings {
    std::string keyFile;
    std::string url = "https://api.deepseek.com/v1/chat/completions";
    std::string model = "deepseek-chat";
    std::string language = "Polish";
    int timeoutSec = 60;
};

struct ExplainRequest {
    std::string level;          // "JLPT N4" / "TOPIK I · 1급 (A1)"
    std::string studyLanguage = "Japanese";
    std::string exampleStyle;   // how to write examples in the study language
    std::string section;        // vocabulary | grammar | reading | listening
    std::string instruction;
    std::string passage;        // reading passage or listening transcript
    std::string question;       // [[x]] markup already converted to 【x】
    std::vector<std::string> options;
    std::string userAnswer;
    std::string correctAnswer;
};

struct TranslateRequest {
    std::string level;
    std::string studyLanguage = "Japanese";
    std::string section;
    std::string passage;        // reading passage or listening transcript
    std::string question;       // sentence with a blank or 【】 markup
    std::string correctAnswer;  // filled into the blank before translating
};

struct ApiResult {
    bool ok = false;
    std::string text;   // explanation on success
    std::string error;  // human readable error otherwise
};

class DeepSeekClient {
public:
    explicit DeepSeekClient(DeepSeekSettings settings);

    bool isConfigured() const { return !apiKey_.empty(); }
    std::string statusLine() const;
    const std::string& language() const { return settings_.language; }
    void setLanguage(const std::string& language) { settings_.language = language; }
    const std::string& keyFile() const { return settings_.keyFile; }
    bool keyFromEnvironment() const;           // $DEEPSEEK_API_KEY wins over the key file
    void setKeyFile(const std::string& path);  // re-reads the credentials

    // Explains a wrong answer, or — when userAnswer == correctAnswer — why the right answer is right.
    ApiResult explain(const ExplainRequest& req) const;
    ApiResult translate(const TranslateRequest& req) const;
    ApiResult chat(const std::string& systemPrompt, const std::string& userPrompt) const;

private:
    void loadCredentials();

    DeepSeekSettings settings_;
    std::string apiKey_;
    std::string credentialsError_;
};
