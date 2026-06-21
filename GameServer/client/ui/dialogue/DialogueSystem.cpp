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
    out.style.textColor = readColor(value.find("textColor"), out.style.textColor);
    out.style.padding = numberOr(value.find("padding"), out.style.padding);
    out.style.fontSize = numberOr(value.find("fontSize"), out.style.fontSize);
    out.style.fadeOutSeconds = std::max(0.01f,
        numberOr(value.find("fadeOutSeconds"), out.style.fadeOutSeconds));

    if (const auto* fontFamily = value.find("fontFamily");
        fontFamily && fontFamily->isString()) {
        const std::wstring family = utf8ToWide(fontFamily->asString());
        if (!family.empty()) out.style.fontFamily = family;
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

    const float innerWidth = std::max(1.f, s.width - s.padding * 2.f);
    const float innerHeight = std::max(1.f, s.height - s.padding * 2.f);
    label_->offsetX = DimValue::px(s.padding);
    label_->offsetY = DimValue::px(s.padding);
    label_->width = DimValue::px(innerWidth);
    label_->height = DimValue::px(innerHeight);
    label_->setFontFamily(s.fontFamily);
    label_->setFontSize(s.fontSize);
    label_->setTextColor(s.textColor.r, s.textColor.g, s.textColor.b, s.textColor.a);
    label_->setText(definition.pages.front());
    label_->colorTint = { 1.f, 1.f, 1.f, 1.f };
    applyAlpha(1.f);
}

void DialogueSystem::advance() {
    if (state_ != State::Reading || !activeDefinition_) return;

    if (pageIndex_ + 1 < activeDefinition_->pages.size()) {
        ++pageIndex_;
        label_->setText(activeDefinition_->pages[pageIndex_]);
        return;
    }

    state_ = State::Fading;
    fadeElapsed_ = 0.f;
}

void DialogueSystem::update(float deltaTimeSec) {
    if (state_ != State::Fading || !activeDefinition_) return;

    fadeElapsed_ += std::max(0.f, deltaTimeSec);
    const float duration = activeDefinition_->style.fadeOutSeconds;
    const float alpha = std::clamp(1.f - fadeElapsed_ / duration, 0.f, 1.f);
    applyAlpha(alpha);
    if (alpha <= 0.f) hide();
}

void DialogueSystem::applyAlpha(float alpha) {
    if (!panel_ || !label_ || !activeDefinition_) return;
    panel_->colorTint.a = activeDefinition_->style.background.a * alpha;
    label_->colorTint.a = alpha;
}

void DialogueSystem::hide() {
    state_ = State::Hidden;
    activeDefinition_ = nullptr;
    activeEventId_.clear();
    pageIndex_ = 0;
    fadeElapsed_ = 0.f;
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
