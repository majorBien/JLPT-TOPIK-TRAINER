#include "Language.h"

namespace {

LanguageProfile makeJapanese() {
    LanguageProfile p;
    p.id = "ja";
    p.name = "Japanese";
    p.nativeName = "日本語";
    p.exam = "JLPT";
    p.banner = {
        "     ██╗██╗     ██████╗ ████████╗",
        "     ██║██║     ██╔══██╗╚══██╔══╝",
        "     ██║██║     ██████╔╝   ██║   ",
        "██   ██║██║     ██╔═══╝    ██║   ",
        "╚█████╔╝███████╗██║        ██║   ",
        " ╚════╝ ╚══════╝╚═╝        ╚═╝   ",
    };
    p.tagline = "日本語能力試験  ·  {level} トレーナー";
    p.subtitle = "Japanese-Language Proficiency Test trainer  ·  がんばって！";
    p.goodbye = "おつかれさまでした！ またね 👋";
    p.menuTitle = "メニュー Menu";
    p.levelTitle = "レベル";
    p.settingsTitle = "設定";
    p.exitTitle = "終了";
    p.modules = {
        {"vocabulary", {"文字・語彙", "Language Knowledge (Vocabulary)"}},
        {"grammar", {"文法・読解", "Language Knowledge (Grammar) & Reading"}},
        {"listening", {"聴解", "Listening"}},
        {"srs", {"復習 SRS", "SRS Vocabulary Trainer"}},
        {"mistakes", {"間違い", "My Mistakes"}},
        {"progress", {"進歩", "Progress"}},
    };
    p.levelsDescending = true;

    p.questionNo = "問";
    p.correct = " ✔ 正解！ Correct ";
    p.incorrect = " ✘ 不正解 Incorrect ";
    p.transcriptTitle = "スクリプト Transcript";
    p.passageTitle = "読解 Reading passage";
    p.listenToAudio = "（音声を聞いて ‧ listen to the audio）";
    p.whyTitle = "なぜ？ Why · DeepSeek";
    p.translationTitle = "翻訳 Translation · DeepSeek";
    p.resultsTitle = "結果  Session results";
    p.cheerPerfect = "素晴らしい！ Perfect!";
    p.cheerGood = "いいですね！ Nice work!";
    p.cheerKeepGoing = "頑張りましょう！ Keep going!";
    p.exampleLabel = "例";
    p.blank = "（　　　）";
    p.furigana = true;
    p.speakListeningQuestion = true;
    p.readingPerSession = 1;
    p.sections = {{"vocabulary", "語彙"}, {"grammar", "文法"}, {"reading", "読解"}, {"listening", "聴解"}};

    p.vocabInstructions = {
        {"kanji_reading", {"＿＿＿の ことばは ひらがなで どう かきますか。", "How is the underlined word read?"}},
        {"orthography", {"＿＿＿の ことばは どう かきますか。", "How is the underlined word written?"}},
        {"context", {"（　　）に なにを いれますか。いちばん いい ものを えらんで ください。", "Which word best fills the blank?"}},
        {"meaning", {"＿＿＿の ことばの いみは なんですか。", "What does the underlined word mean?"}},
        {"reverse_meaning", {"つぎの いみの ことばは どれですか。", "Which word has this meaning?"}},
    };
    p.defaultInstruction = {"つぎの 文の （　　）に 何を 入れますか。", "Choose the best option for the blank."};

    p.listeningOrder = {"task", "point", "utterance", "response"};
    p.listeningTypes = {{"task", "課題理解 (Task)"}, {"point", "ポイント理解 (Key point)"},
                        {"utterance", "発話表現 (Utterance)"}, {"response", "即時応答 (Quick response)"}};
    p.listeningInstruction = "もんだいを きいて、いちばん いい ものを えらんで ください。";
    p.listeningPrompt = "いちばん いい ものは どれですか。";
    p.listeningHint = "In 発話表現/即時応答 the answer choices are only spoken — listen carefully!";
    p.speakers = {{"M", "男"}, {"F", "女"}, {"N", "ナレーター"}};
    p.speakerSeparator = "：";
    p.spokenNumbers = {"いち", "に", "さん", "よん", "ご"};
    p.preparingAudio = "音声を準備中… preparing audio";
    p.playing = "♪ 再生中… playing";
    p.finished = "♪ 再生終了 finished";

    p.wellDone = "お疲れさま！";
    p.reviewDone = "復習 完了  Review finished";
    p.perfectNoMistakes = "完璧！";
    p.exampleStyle = "Japanese examples in Japanese with readings in parentheses where helpful";
    return p;
}

LanguageProfile makeKorean() {
    LanguageProfile p;
    p.id = "ko";
    p.name = "Korean";
    p.nativeName = "한국어";
    p.exam = "TOPIK I";
    p.banner = {
        "████████╗ ██████╗ ██████╗ ██╗██╗  ██╗",
        "╚══██╔══╝██╔═══██╗██╔══██╗██║██║ ██╔╝",
        "   ██║   ██║   ██║██████╔╝██║█████╔╝ ",
        "   ██║   ██║   ██║██╔═══╝ ██║██╔═██╗ ",
        "   ██║   ╚██████╔╝██║     ██║██║  ██╗",
        "   ╚═╝    ╚═════╝ ╚═╝     ╚═╝╚═╝  ╚═╝",
    };
    p.tagline = "한국어능력시험  ·  {level} 트레이너";
    p.subtitle = "Test of Proficiency in Korean trainer  ·  화이팅!";
    p.goodbye = "수고하셨습니다! 또 만나요 👋";
    p.menuTitle = "메뉴 Menu";
    p.levelTitle = "레벨";
    p.settingsTitle = "설정";
    p.exitTitle = "종료";
    p.modules = {
        {"vocabulary", {"어휘", "Vocabulary (어휘)"}},
        {"grammar", {"읽기", "Reading (읽기) · grammar & texts"}},
        {"listening", {"듣기", "Listening (듣기)"}},
        {"srs", {"복습 SRS", "SRS Vocabulary Trainer"}},
        {"mistakes", {"오답 노트", "My Mistakes"}},
        {"progress", {"학습 현황", "Progress"}},
    };
    p.levelsDescending = false;
    p.levelNames = {{"A1", "TOPIK I · 1급 (A1)"}, {"A2", "TOPIK I · 2급 (A2)"}};

    p.questionNo = "문제";
    p.correct = " ✔ 정답! Correct ";
    p.incorrect = " ✘ 오답 Incorrect ";
    p.transcriptTitle = "대본 Transcript";
    p.passageTitle = "지문 Reading passage";
    p.listenToAudio = "(음성을 들으세요 ‧ listen to the audio)";
    p.whyTitle = "왜? Why · DeepSeek";
    p.translationTitle = "번역 Translation · DeepSeek";
    p.resultsTitle = "결과  Session results";
    p.cheerPerfect = "완벽해요! Perfect!";
    p.cheerGood = "잘했어요! Nice work!";
    p.cheerKeepGoing = "힘내세요! Keep going!";
    p.exampleLabel = "예";
    p.blank = "(　　　)";
    p.furigana = false;
    p.speakListeningQuestion = false;
    p.readingPerSession = 2;
    p.sections = {{"vocabulary", "어휘"}, {"grammar", "문법"}, {"reading", "읽기"}, {"listening", "듣기"}};

    p.vocabInstructions = {
        {"context", {"(　　)에 들어갈 가장 알맞은 것을 고르십시오.", "Choose the word that best fills the blank."}},
        {"meaning", {"밑줄 친 단어의 뜻으로 알맞은 것을 고르십시오.", "What does the underlined word mean?"}},
        {"reverse_meaning", {"다음 뜻에 맞는 단어를 고르십시오.", "Which word has this meaning?"}},
    };
    p.defaultInstruction = {"알맞은 것을 고르십시오.", "Choose the best answer."};

    // TOPIK I 듣기 parts in exam order (questions 1–30).
    p.listeningOrder = {"answer", "next", "place", "topic", "match", "main_idea", "detail"};
    p.listeningTypes = {{"answer", "알맞은 대답 (Answer)"}, {"next", "이어지는 말 (Next line)"},
                        {"place", "장소 (Place)"}, {"topic", "화제 (Topic)"},
                        {"match", "내용 일치 (Details)"}, {"main_idea", "중심 생각 (Main idea)"},
                        {"detail", "담화 이해 (Longer talk)"}};
    p.listeningInstructions = {
        {"answer", "다음을 듣고 물음에 맞는 대답을 고르십시오."},
        {"next", "다음을 듣고 이어지는 말을 고르십시오."},
        {"place", "여기는 어디입니까? 알맞은 것을 고르십시오."},
        {"topic", "다음은 무엇에 대해 말하고 있습니까? 알맞은 것을 고르십시오."},
        {"match", "다음을 듣고 들은 내용과 같은 것을 고르십시오."},
        {"main_idea", "다음을 듣고 중심 생각을 고르십시오."},
        {"detail", "다음을 듣고 물음에 답하십시오."},
    };
    p.listeningInstruction = "다음을 듣고 알맞은 것을 고르십시오.";
    p.listeningPrompt = "알맞은 것을 고르십시오.";
    p.listeningHint = "As in TOPIK, the question and the answer choices are printed — only the dialogue is spoken.";
    p.speakers = {{"M", "남자"}, {"F", "여자"}, {"N", "안내"}};
    p.speakerSeparator = ": ";
    p.spokenNumbers = {"일 번", "이 번", "삼 번", "사 번"};
    p.preparingAudio = "음성 준비 중… preparing audio";
    p.playing = "♪ 재생 중… playing";
    p.finished = "♪ 재생 끝 finished";

    p.wellDone = "수고했어요!";
    p.reviewDone = "복습 완료  Review finished";
    p.perfectNoMistakes = "완벽해요!";
    p.exampleStyle = "Korean examples in Hangul (no romanization unless it really helps)";
    return p;
}

}  // namespace

const LanguageProfile& languageProfile(const std::string& id) {
    static const LanguageProfile ja = makeJapanese();
    static const LanguageProfile ko = makeKorean();
    return id == "ko" ? ko : ja;
}

const std::vector<std::string>& languageIds() {
    static const std::vector<std::string> ids = {"ja", "ko"};
    return ids;
}

std::string LanguageProfile::levelLabel(const std::string& level) const {
    const auto it = levelNames.find(level);
    return it != levelNames.end() ? it->second : level;
}

std::string LanguageProfile::moduleNative(const std::string& key) const {
    const auto it = modules.find(key);
    return it != modules.end() ? it->second.first : key;
}

std::string LanguageProfile::moduleEnglish(const std::string& key) const {
    const auto it = modules.find(key);
    return it != modules.end() ? it->second.second : key;
}

std::string LanguageProfile::section(const std::string& key) const {
    const auto it = sections.find(key);
    return it != sections.end() ? it->second : key;
}

std::string LanguageProfile::listeningTypeLabel(const std::string& type) const {
    const auto it = listeningTypes.find(type);
    return it != listeningTypes.end() ? it->second : type;
}
