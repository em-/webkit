/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2009, 2015 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef HTMLElement_h
#define HTMLElement_h

#include "StyledElement.h"

namespace WebCore {

class DocumentFragment;
class FormNamedItem;
class HTMLCollection;
class HTMLFormElement;

enum TranslateAttributeMode {
    TranslateAttributeYes,
    TranslateAttributeNo,
    TranslateAttributeInherit
};

class HTMLElement : public StyledElement {
public:
    static Ref<HTMLElement> create(const QualifiedName& tagName, Document&);

    WEBCORE_EXPORT String title() const final;

    int tabIndex() const override;

    void setInnerText(const String&, ExceptionCode&);
    void setOuterText(const String&, ExceptionCode&);

    Element* insertAdjacentElement(const String& where, Element& newChild, ExceptionCode&);
    void insertAdjacentHTML(const String& where, const String& html, ExceptionCode&);
    void insertAdjacentText(const String& where, const String& text, ExceptionCode&);

    virtual bool hasCustomFocusLogic() const;
    bool supportsFocus() const override;

    String contentEditable() const;
    void setContentEditable(const String&, ExceptionCode&);

    static Editability editabilityFromContentEditableAttr(const Node&);

    virtual bool draggable() const;
    void setDraggable(bool);

    bool spellcheck() const;
    void setSpellcheck(bool);

    bool translate() const;
    void setTranslate(bool);

    void click();

    void accessKeyAction(bool sendMouseEvents) override;

    bool ieForbidsInsertHTML() const;

    bool rendererIsNeeded(const RenderStyle&) override;
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;

    HTMLFormElement* form() const { return virtualForm(); }

    const AtomicString& dir() const;
    void setDir(const AtomicString&);

    bool hasDirectionAuto() const;
    TextDirection directionalityIfhasDirAutoAttribute(bool& isAuto) const;

    virtual bool isHTMLUnknownElement() const { return false; }
    virtual bool isTextControlInnerTextElement() const { return false; }
    virtual bool canHaveUserAgentShadowRoot() const { return false; }

    bool willRespondToMouseMoveEvents() override;
    bool willRespondToMouseWheelEvents() override;
    bool willRespondToMouseClickEvents() override;

    virtual bool isLabelable() const { return false; }
    virtual FormNamedItem* asFormNamedItem() { return 0; }

    bool hasTagName(const HTMLQualifiedName& name) const { return hasLocalName(name.localName()); }

    static const AtomicString& eventNameForEventHandlerAttribute(const QualifiedName& attributeName);

protected:
    HTMLElement(const QualifiedName& tagName, Document&, ConstructionType);

    void addHTMLLengthToStyle(MutableStyleProperties&, CSSPropertyID, const String& value);
    void addHTMLColorToStyle(MutableStyleProperties&, CSSPropertyID, const String& color);

    void applyAlignmentAttributeToStyle(const AtomicString&, MutableStyleProperties&);
    void applyBorderAttributeToStyle(const AtomicString&, MutableStyleProperties&);

    bool matchesReadWritePseudoClass() const override;
    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    bool isPresentationAttribute(const QualifiedName&) const override;
    void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStyleProperties&) override;
    unsigned parseBorderWidthAttribute(const AtomicString&) const;

    void childrenChanged(const ChildChange&) override;
    void calculateAndAdjustDirectionality();

    typedef HashMap<AtomicStringImpl*, AtomicString> EventHandlerNameMap;
    template<size_t tableSize> static void populateEventHandlerNameMap(EventHandlerNameMap&, const QualifiedName* const (&table)[tableSize]);
    static const AtomicString& eventNameForEventHandlerAttribute(const QualifiedName& attributeName, const EventHandlerNameMap&);

private:
    String nodeName() const final;

    void mapLanguageAttributeToLocale(const AtomicString&, MutableStyleProperties&);

    virtual HTMLFormElement* virtualForm() const;

    Node* insertAdjacent(const String& where, Ref<Node>&& newChild, ExceptionCode&);
    Ref<DocumentFragment> textToFragment(const String&, ExceptionCode&);

    void dirAttributeChanged(const AtomicString&);
    void adjustDirectionalityIfNeededAfterChildAttributeChanged(Element* child);
    void adjustDirectionalityIfNeededAfterChildrenChanged(Element* beforeChange, ChildChangeType);
    TextDirection directionality(Node** strongDirectionalityTextNode= 0) const;

    TranslateAttributeMode translateAttributeMode() const;

    static void populateEventHandlerNameMap(EventHandlerNameMap&, const QualifiedName* const table[], size_t tableSize);
    static EventHandlerNameMap createEventHandlerNameMap();
};

inline HTMLElement::HTMLElement(const QualifiedName& tagName, Document& document, ConstructionType type = CreateHTMLElement)
    : StyledElement(tagName, document, type)
{
    ASSERT(tagName.localName().impl());
}

template<size_t tableSize> inline void HTMLElement::populateEventHandlerNameMap(EventHandlerNameMap& map, const QualifiedName* const (&table)[tableSize])
{
    populateEventHandlerNameMap(map, table, tableSize);
}

inline bool Node::hasTagName(const HTMLQualifiedName& name) const
{
    return is<HTMLElement>(*this) && downcast<HTMLElement>(*this).hasTagName(name);
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::HTMLElement)
    static bool isType(const WebCore::Node& node) { return node.isHTMLElement(); }
SPECIALIZE_TYPE_TRAITS_END()

#include "HTMLElementTypeHelpers.h"

#endif // HTMLElement_h
