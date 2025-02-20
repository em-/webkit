/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "KeyframeList.h"

#include "Animation.h"
#include "RenderObject.h"

namespace WebCore {

TimingFunction* KeyframeValue::timingFunction(const AtomicString& name) const
{
    const RenderStyle* keyframeStyle = style();
    if (!keyframeStyle || !keyframeStyle->animations())
        return nullptr;

    for (size_t i = 0; i < keyframeStyle->animations()->size(); ++i) {
        const Animation& animation = keyframeStyle->animations()->animation(i);
        if (name == animation.name())
            return animation.timingFunction().get();
    }

    return nullptr;
}

KeyframeList::~KeyframeList()
{
    clear();
}

void KeyframeList::clear()
{
    m_keyframes.clear();
    m_properties.clear();
}

bool KeyframeList::operator==(const KeyframeList& o) const
{
    if (m_keyframes.size() != o.m_keyframes.size())
        return false;

    Vector<KeyframeValue>::const_iterator it2 = o.m_keyframes.begin();
    for (Vector<KeyframeValue>::const_iterator it1 = m_keyframes.begin(); it1 != m_keyframes.end(); ++it1) {
        if (it1->key() != it2->key())
            return false;
        const RenderStyle& style1 = *it1->style();
        const RenderStyle& style2 = *it2->style();
        if (style1 != style2)
            return false;
        ++it2;
    }

    return true;
}

void KeyframeList::insert(KeyframeValue&& keyframe)
{
    if (keyframe.key() < 0 || keyframe.key() > 1)
        return;

    bool inserted = false;
    bool replaced = false;
    size_t i = 0;
    for (; i < m_keyframes.size(); ++i) {
        if (m_keyframes[i].key() == keyframe.key()) {
            m_keyframes[i] = WTFMove(keyframe);
            replaced = true;
            break;
        }

        if (m_keyframes[i].key() > keyframe.key()) {
            // insert before
            m_keyframes.insert(i, WTFMove(keyframe));
            inserted = true;
            break;
        }
    }
    
    if (!replaced && !inserted)
        m_keyframes.append(WTFMove(keyframe));

    if (replaced) {
        // We have to rebuild the properties list from scratch.
        m_properties.clear();
        for (auto& keyframeValue : m_keyframes) {
            for (auto& property : keyframeValue.properties())
                m_properties.add(property);
        }
        return;
    }

    for (auto& property : m_keyframes[i].properties())
        m_properties.add(property);
}

} // namespace WebCore
