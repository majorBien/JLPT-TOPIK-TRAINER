#include "Question.h"

#include "Language.h"

json::Value Question::toJson() const {
    json::Value v = json::Value::object();
    v.set("id", id);
    v.set("section", section);
    v.set("type", type);
    v.set("instruction", instruction);
    v.set("instruction_en", instructionEn);
    if (!passage.empty()) v.set("passage", passage);
    v.set("prompt", prompt);
    json::Value opts = json::Value::array();
    for (const auto& o : options) opts.push_back(o);
    v.set("options", std::move(opts));
    v.set("answer", answer);
    if (!note.empty()) v.set("note", note);
    v.set("track_key", trackKey);
    v.set("track_label", trackLabel);
    v.set("track_group", trackGroup);
    return v;
}

Question Question::fromJson(const json::Value& v) {
    Question q;
    q.id = v.getString("id");
    q.section = v.getString("section");
    q.type = v.getString("type");
    q.instruction = v.getString("instruction");
    q.instructionEn = v.getString("instruction_en");
    q.passage = v.getString("passage");
    q.prompt = v.getString("prompt");
    q.options = v.getStringArray("options");
    q.answer = static_cast<int>(v.getInt("answer"));
    q.note = v.getString("note");
    q.trackKey = v.getString("track_key");
    q.trackLabel = v.getString("track_label");
    q.trackGroup = v.getString("track_group");
    return q;
}

VocabEntry VocabEntry::fromJson(const json::Value& v) {
    VocabEntry e;
    e.id = v.getString("id");
    e.word = v.getString("word");
    e.reading = v.getString("reading");
    e.meaning = v.getString("meaning");
    e.pos = v.getString("pos");
    e.example = v.getString("example");
    e.form = v.getString("form", e.word);
    e.kanjiDistractors = v.getStringArray("kanji_distractors");
    return e;
}

std::string ListeningItem::transcript(const LanguageProfile& lang) const {
    auto who = [&](const std::string& s) {
        const auto it = lang.speakers.find(s);
        return (it != lang.speakers.end() ? it->second : s) + lang.speakerSeparator;
    };
    std::string out;
    if (!situation.empty()) out += who("N") + situation + "\n";
    for (const auto& l : lines) out += who(l.speaker) + l.text + "\n";
    if (spokenOptions()) {
        if (!question.empty()) out += who("N") + question + "\n";
        for (std::size_t i = 0; i < options.size(); ++i)
            out += who(optionSpeaker) + std::to_string(i + 1) + ". " + options[i] + "\n";
    } else if (!question.empty() && lang.speakListeningQuestion) {
        out += who("N") + question + "\n";
    }
    if (!out.empty()) out.pop_back();
    return out;
}

Question ListeningItem::toQuestion(const LanguageProfile& lang) const {
    Question q;
    q.id = "listening:" + id;
    q.section = "listening";
    q.type = type;
    const auto it = lang.listeningInstructions.find(type);
    q.instruction = it != lang.listeningInstructions.end() ? it->second : lang.listeningInstruction;
    q.instructionEn = "Listen and choose the best answer. (" + lang.listeningTypeLabel(type) + ")";
    q.passage = transcript(lang);
    // 概要理解: the question is only heard (it is in the transcript shown after answering).
    q.prompt = question.empty() || type == "summary" ? lang.listeningPrompt : question;
    q.options = options;
    q.answer = answer;
    q.note = note;
    q.trackKey = "listening:" + type;
    q.trackLabel = lang.listeningTypeLabel(type);
    q.trackGroup = group::Listening;
    return q;
}

ListeningItem ListeningItem::fromJson(const json::Value& v) {
    ListeningItem item;
    item.id = v.getString("id");
    item.type = v.getString("type", "task");
    item.situation = v.getString("situation");
    item.question = v.getString("question");
    for (const auto& l : v["lines"].asArray()) item.lines.push_back({l.getString("speaker", "N"), l.getString("text")});
    item.options = v.getStringArray("options");
    item.answer = static_cast<int>(v.getInt("answer"));
    item.optionSpeaker = v.getString("option_speaker", "M");
    item.note = v.getString("note");
    return item;
}
