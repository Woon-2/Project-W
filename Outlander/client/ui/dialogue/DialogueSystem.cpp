#include "pch.hpp"
#include "DialogueSystem.hpp"
#include "../../../common/simpleJson.hpp"
#include "../../log.hpp"

namespace UI {
namespace {

float numberOr(const json::Value* value, float fallback) {
    return value && value->isNumber()
        ? static_cast<float>(value->asNumber(fallback))
        : fallback;
}

std::wstring utf8ToWide(std::string_view text) {
    constexpr char32_t replacement = 0xFFFD;
    std::wstring result;
    result.reserve(text.size());

    auto appendCodePoint = [&](char32_t codePoint) {
        if constexpr (sizeof(wchar_t) == 2) {
            if (codePoint <= 0xFFFF) {
                result.push_back(static_cast<wchar_t>(codePoint));
                return;
            }
            codePoint -= 0x10000;
            result.push_back(static_cast<wchar_t>(0xD800 + (codePoint >> 10)));
            result.push_back(static_cast<wchar_t>(0xDC00 + (codePoint & 0x3FF)));
        } else {
            result.push_back(static_cast<wchar_t>(codePoint));
        }
    };

    std::size_t offset = 0;
    while (offset < text.size()) {
        const auto first = static_cast<unsigned char>(text[offset]);
        char32_t codePoint = replacement;
        std::size_t sequenceLength = 1;

        if (first < 0x80) {
            codePoint = first;
        } else if ((first & 0xE0) == 0xC0) {
            codePoint = first & 0x1F;
            sequenceLength = 2;
        } else if ((first & 0xF0) == 0xE0) {
            codePoint = first & 0x0F;
            sequenceLength = 3;
        } else if ((first & 0xF8) == 0xF0) {
            codePoint = first & 0x07;
            sequenceLength = 4;
        }

        bool valid = sequenceLength <= text.size() - offset;
        for (std::size_t i = 1; valid && i < sequenceLength; ++i) {
            const auto continuation = static_cast<unsigned char>(text[offset + i]);
            valid = (continuation & 0xC0) == 0x80;
            if (valid) codePoint = (codePoint << 6) | (continuation & 0x3F);
        }

        const bool overlong =
            (sequenceLength == 2 && codePoint < 0x80) ||
            (sequenceLength == 3 && codePoint < 0x800) ||
            (sequenceLength == 4 && codePoint < 0x10000);
        const bool invalidCodePoint =
            codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF);

        if (!valid || overlong || invalidCodePoint) {
            appendCodePoint(replacement);
            ++offset;
            continue;
        }

        appendCodePoint(codePoint);
        offset += sequenceLength;
    }
    return result;
}

Color readColor(const json::Value* value, Color fallback) {
    if (!value || !value->isArray()) return fallback;
    const auto& a = value->asArray();
    if (a.size() < 3) return fallback;
    fallback.r = numberOr(&a[0], fallback.r);
    fallback.g = numberOr(&a[1], fallback.g);
    fallback.b = numberOr(&a[2], fallback.b);
    if (a.size() >= 4) fallback.a = numberOr(&a[3], fallback.a);
    return fallback;
}

bool parseDefinition(const json::Value& value, DialogueDefinition& out) {
    if (!value.isObject()) return false;

    const auto* eventId = value.find("eventId");
    const auto* pages = value.find("pages");
    if (!eventId || !eventId->isString() || !pages || !pages->isArray()) return false;

    out.eventId = eventId->asString();
    if (out.eventId.empty()) return false;

    if (const auto* rect = value.find("rect"); rect && rect->isObject()) {
        out.style.x = numberOr(rect->find("x"), out.style.x);
        out.style.y = numberOr(rect->find("y"), out.style.y);
        out.style.width = numberOr(rect->find("width"), out.style.width);
        out.style.height = numberOr(rect->find("height"), out.style.height);
    }

    out.style.background = readColor(value.find("background"), out.style.background);
    out.style.borderColor = readColor(value.find("borderColor"), out.style.borderColor);
    out.style.borderWidth = std::max(0.f,
        numberOr(value.find("borderWidth"), out.style.borderWidth));
    out.style.textColor = readColor(value.find("textColor"), out.style.textColor);
    out.style.hintColor = readColor(value.find("hintColor"), out.style.hintColor);
    out.style.padding = numberOr(value.find("padding"), out.style.padding);
    out.style.fontSize = numberOr(value.find("fontSize"), out.style.fontSize);
    out.style.hintFontSize = std::max(1.f,
        numberOr(value.find("hintFontSize"), out.style.hintFontSize));
    out.style.charactersPerSecond = std::max(1.f,
        numberOr(value.find("charactersPerSecond"), out.style.charactersPerSecond));
    out.style.fadeOutSeconds = std::max(0.01f,
        numberOr(value.find("fadeOutSeconds"), out.style.fadeOutSeconds));

    if (const auto* fontFamily = value.find("fontFamily");
        fontFamily && fontFamily->isString()) {
        const std::wstring family = utf8ToWide(fontFamily->asString());
        if (!family.empty()) out.style.fontFamily = family;
    }

    if (const auto* advanceHint = value.find("advanceHint");
        advanceHint && advanceHint->isString()) {
        const std::wstring text = utf8ToWide(advanceHint->asString());
        if (!text.empty()) out.advanceHint = text;
    }
    if (const auto* closeHint = value.find("closeHint");
        closeHint && closeHint->isString()) {
        const std::wstring text = utf8ToWide(closeHint->asString());
        if (!text.empty()) out.closeHint = text;
    }

    for (const auto& page : pages->asArray()) {
        if (!page.isString()) continue;
        out.pages.push_back(utf8ToWide(page.asString()));
    }
    return !out.pages.empty();
}

} // namespace

bool DialogueSystem::init(UIManager& uiManager, const std::filesystem::path& jsonPath) {
    uiManager_ = &uiManager;
    jsonPath_ = jsonPath;

    // Build the widget tree once. A re-init (e.g. re-entering in-game in online
    // mode) reuses the existing widgets and just refreshes the JSON definitions.
    if (!panel_) {
        panel_ = static_cast<Panel*>(
            uiManager.root()->addChild(std::make_unique<Panel>())
        );
        panel_->name = "DialogueWindow";
        panel_->anchor = Anchors::TopLeft;
        panel_->pivot = Pivots::TopLeft;
        panel_->zOrder = 10000;
        panel_->visible = false;
        panel_->interactive = false;
        panel_->drawSolidBackground = true;

        label_ = static_cast<Label*>(
            panel_->addChild(std::make_unique<Label>())
        );
        label_->name = "DialogueText";
        label_->anchor = Anchors::TopLeft;
        label_->pivot = Pivots::TopLeft;
        label_->setTextHAlign(TextHAlign::Leading);
        label_->setTextVAlign(TextVAlign::Top);
        label_->zOrder = 1;

        hintLabel_ = static_cast<Label*>(
            panel_->addChild(std::make_unique<Label>())
        );
        hintLabel_->name = "DialogueAdvanceHint";
        hintLabel_->anchor = Anchors::TopLeft;
        hintLabel_->pivot = Pivots::TopLeft;
        hintLabel_->setTextHAlign(TextHAlign::Trailing);
        hintLabel_->setTextVAlign(TextVAlign::Bottom);
        hintLabel_->zOrder = 1;

        constexpr const char* borderNames[] = {
            "DialogueBorderTop",
            "DialogueBorderBottom",
            "DialogueBorderLeft",
            "DialogueBorderRight"
        };
        for (std::size_t i = 0; i < borderPanels_.size(); ++i) {
            borderPanels_[i] = static_cast<Panel*>(
                panel_->addChild(std::make_unique<Panel>())
            );
            borderPanels_[i]->name = borderNames[i];
            borderPanels_[i]->anchor = Anchors::TopLeft;
            borderPanels_[i]->pivot = Pivots::TopLeft;
            borderPanels_[i]->zOrder = 2;
            borderPanels_[i]->interactive = false;
            borderPanels_[i]->drawSolidBackground = true;
        }
    }

    return loadDefinitions();
}

bool DialogueSystem::reload() {
    const std::string eventToRestore = activeEventId_;
    const bool wasActive = active();
    if (!loadDefinitions()) return false;
    if (wasActive && !eventToRestore.empty()) return show(eventToRestore);
    return true;
}

bool DialogueSystem::loadDefinitions() {
    std::ifstream input(jsonPath_, std::ios::binary);
    if (!input) {
        gSharedLog << "[Dialogue] failed to open " << jsonPath_.string() << "\n";
        return false;
    }

    std::string text(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>()
    );
    if (text.size() >= 3 &&
        static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }

    json::Value root;
    std::string error;
    if (!json::parse(text, root, &error)) {
        gSharedLog << "[Dialogue] JSON parse failed: " << error << "\n";
        return false;
    }

    const auto* dialogues = root.find("dialogues");
    if (!dialogues || !dialogues->isArray()) {
        gSharedLog << "[Dialogue] missing dialogues array\n";
        return false;
    }

    std::unordered_map<std::string, DialogueDefinition> loaded;
    for (const auto& value : dialogues->asArray()) {
        DialogueDefinition definition;
        if (!parseDefinition(value, definition)) continue;
        loaded.insert_or_assign(definition.eventId, std::move(definition));
    }

    if (loaded.empty()) {
        gSharedLog << "[Dialogue] no valid definitions\n";
        return false;
    }

    definitions_ = std::move(loaded);
    return true;
}

bool DialogueSystem::show(std::string_view eventId) {
    const auto it = definitions_.find(std::string(eventId));
    if (it == definitions_.end()) return false;

    activeDefinition_ = &it->second;
    activeEventId_ = it->first;
    pageIndex_ = 0;
    fadeElapsed_ = 0.f;
    hintPulseElapsed_ = 0.f;
    revealedTextLength_ = 0;
    typewriterAccumulator_ = 0.f;
    state_ = State::Reading;
    applyDefinition(*activeDefinition_);
    panel_->visible = true;
    return true;
}

void DialogueSystem::applyDefinition(const DialogueDefinition& definition) {
    const auto& s = definition.style;
    panel_->offsetX = DimValue::px(s.x);
    panel_->offsetY = DimValue::px(s.y);
    panel_->width = DimValue::px(s.width);
    panel_->height = DimValue::px(s.height);
    panel_->colorTint = s.background;

    const float borderWidth = std::clamp(
        s.borderWidth, 0.f, std::min(s.width, s.height) * 0.5f);
    for (Panel* border : borderPanels_) {
        border->visible = borderWidth > 0.f;
        border->colorTint = s.borderColor;
    }

    // Top, bottom, left, right. The border is inset so the authored dialogue
    // rectangle keeps its original footprint and remains screen-safe.
    borderPanels_[0]->offsetX = DimValue::px(0.f);
    borderPanels_[0]->offsetY = DimValue::px(0.f);
    borderPanels_[0]->width = DimValue::px(s.width);
    borderPanels_[0]->height = DimValue::px(borderWidth);

    borderPanels_[1]->offsetX = DimValue::px(0.f);
    borderPanels_[1]->offsetY = DimValue::px(s.height - borderWidth);
    borderPanels_[1]->width = DimValue::px(s.width);
    borderPanels_[1]->height = DimValue::px(borderWidth);

    borderPanels_[2]->offsetX = DimValue::px(0.f);
    borderPanels_[2]->offsetY = DimValue::px(borderWidth);
    borderPanels_[2]->width = DimValue::px(borderWidth);
    borderPanels_[2]->height = DimValue::px(std::max(0.f, s.height - borderWidth * 2.f));

    borderPanels_[3]->offsetX = DimValue::px(s.width - borderWidth);
    borderPanels_[3]->offsetY = DimValue::px(borderWidth);
    borderPanels_[3]->width = DimValue::px(borderWidth);
    borderPanels_[3]->height = DimValue::px(std::max(0.f, s.height - borderWidth * 2.f));

    const float innerWidth = std::max(1.f, s.width - s.padding * 2.f);
    const float innerHeight = std::max(1.f, s.height - s.padding * 2.f);
    const float hintHeight = std::min(
        innerHeight, std::max(1.f, s.hintFontSize * 1.6f));
    constexpr float kHintGap = 8.f;
    const float bodyHeight = std::max(
        1.f, innerHeight - hintHeight - kHintGap);

    label_->offsetX = DimValue::px(s.padding);
    label_->offsetY = DimValue::px(s.padding);
    label_->width = DimValue::px(innerWidth);
    label_->height = DimValue::px(bodyHeight);
    label_->setFontFamily(s.fontFamily);
    label_->setFontSize(s.fontSize);
    label_->setTextColor(s.textColor.r, s.textColor.g, s.textColor.b, s.textColor.a);
    label_->colorTint = { 1.f, 1.f, 1.f, 1.f };

    hintLabel_->offsetX = DimValue::px(s.padding);
    hintLabel_->offsetY = DimValue::px(s.height - s.padding - hintHeight);
    hintLabel_->width = DimValue::px(innerWidth);
    hintLabel_->height = DimValue::px(hintHeight);
    hintLabel_->setFontFamily(s.fontFamily);
    hintLabel_->setFontSize(s.hintFontSize);
    hintLabel_->setTextColor(
        s.hintColor.r, s.hintColor.g, s.hintColor.b, s.hintColor.a);
    hintLabel_->colorTint = { 1.f, 1.f, 1.f, 1.f };
    beginPage();
    applyAlpha(1.f);
}

void DialogueSystem::beginPage() {
    if (!label_ || !activeDefinition_
        || pageIndex_ >= activeDefinition_->pages.size()) {
        return;
    }

    revealedTextLength_ = 0;
    typewriterAccumulator_ = 0.f;
    label_->setText(L"");
    updateHintText();
}

bool DialogueSystem::currentPageFullyRevealed() const {
    if (!activeDefinition_ || pageIndex_ >= activeDefinition_->pages.size()) {
        return true;
    }
    return revealedTextLength_ >= activeDefinition_->pages[pageIndex_].size();
}

void DialogueSystem::revealCurrentPage() {
    if (!label_ || !activeDefinition_
        || pageIndex_ >= activeDefinition_->pages.size()) {
        return;
    }

    const std::wstring& page = activeDefinition_->pages[pageIndex_];
    revealedTextLength_ = page.size();
    typewriterAccumulator_ = 0.f;
    label_->setText(page);
}

void DialogueSystem::updateTypewriter(float deltaTimeSec) {
    if (currentPageFullyRevealed() || !activeDefinition_) return;

    typewriterAccumulator_ +=
        deltaTimeSec * activeDefinition_->style.charactersPerSecond;
    std::size_t charactersToReveal =
        static_cast<std::size_t>(typewriterAccumulator_);
    if (charactersToReveal == 0) return;
    typewriterAccumulator_ -= static_cast<float>(charactersToReveal);

    const std::wstring& page = activeDefinition_->pages[pageIndex_];
    while (charactersToReveal-- > 0 && revealedTextLength_ < page.size()) {
        // Keep UTF-16 surrogate pairs together so supplementary characters
        // never appear as a broken half-glyph during the reveal.
        const wchar_t current = page[revealedTextLength_++];
        if (current >= 0xD800 && current <= 0xDBFF
            && revealedTextLength_ < page.size()) {
            const wchar_t next = page[revealedTextLength_];
            if (next >= 0xDC00 && next <= 0xDFFF) {
                ++revealedTextLength_;
            }
        }
    }

    label_->setText(page.substr(0, revealedTextLength_));
}

void DialogueSystem::updateHintText() {
    if (!hintLabel_ || !activeDefinition_) return;
    const bool lastPage = pageIndex_ + 1 >= activeDefinition_->pages.size();
    hintLabel_->setText(
        lastPage ? activeDefinition_->closeHint : activeDefinition_->advanceHint);
}

void DialogueSystem::advance() {
    if (state_ != State::Reading || !activeDefinition_) return;

    if (!currentPageFullyRevealed()) {
        revealCurrentPage();
        return;
    }

    if (pageIndex_ + 1 < activeDefinition_->pages.size()) {
        ++pageIndex_;
        beginPage();
        return;
    }

    state_ = State::Fading;
    fadeElapsed_ = 0.f;
}

void DialogueSystem::update(float deltaTimeSec) {
    if (state_ == State::Hidden || !activeDefinition_) return;

    const float delta = std::max(0.f, deltaTimeSec);
    hintPulseElapsed_ += delta;
    if (state_ == State::Reading) {
        updateTypewriter(delta);
        applyAlpha(1.f);
        return;
    }

    fadeElapsed_ += delta;
    const float duration = activeDefinition_->style.fadeOutSeconds;
    const float alpha = std::clamp(1.f - fadeElapsed_ / duration, 0.f, 1.f);
    applyAlpha(alpha);
    if (alpha <= 0.f) hide();
}

void DialogueSystem::applyAlpha(float alpha) {
    if (!panel_ || !label_ || !hintLabel_ || !activeDefinition_) return;
    panel_->colorTint.a = activeDefinition_->style.background.a * alpha;
    for (Panel* border : borderPanels_) {
        border->colorTint.a = activeDefinition_->style.borderColor.a * alpha;
    }
    label_->colorTint.a = alpha;
    constexpr float kPulseRadiansPerSecond = 4.5f;
    const float pulse = 0.75f
        + 0.25f * std::cos(hintPulseElapsed_ * kPulseRadiansPerSecond);
    hintLabel_->colorTint.a = pulse * alpha;
}

void DialogueSystem::hide() {
    state_ = State::Hidden;
    activeDefinition_ = nullptr;
    activeEventId_.clear();
    pageIndex_ = 0;
    fadeElapsed_ = 0.f;
    hintPulseElapsed_ = 0.f;
    revealedTextLength_ = 0;
    typewriterAccumulator_ = 0.f;
    if (panel_) panel_->visible = false;
}

bool DialogueSystem::handleWndMsg(UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!active()) return false;

    if (msg == WM_KEYDOWN && wParam == VK_RETURN) {
        if ((lParam & (1LL << 30)) == 0) advance();
        return true;
    }
    if (msg == WM_LBUTTONDOWN) {
        advance();
        return true;
    }
    return msg == WM_LBUTTONUP;
}

} // namespace UI
