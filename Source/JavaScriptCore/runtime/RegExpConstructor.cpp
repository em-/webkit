/*
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007-2008, 2016 Apple Inc. All Rights Reserved.
 *  Copyright (C) 2009 Torch Mobile, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include "RegExpConstructor.h"

#include "Error.h"
#include "GetterSetter.h"
#include "JSCInlines.h"
#include "RegExpMatchesArray.h"
#include "RegExpPrototype.h"
#include "StructureInlines.h"

namespace JSC {

static EncodedJSValue regExpConstructorInput(ExecState*, EncodedJSValue, PropertyName);
static EncodedJSValue regExpConstructorMultiline(ExecState*, EncodedJSValue, PropertyName);
static EncodedJSValue regExpConstructorLastMatch(ExecState*, EncodedJSValue, PropertyName);
static EncodedJSValue regExpConstructorLastParen(ExecState*, EncodedJSValue, PropertyName);
static EncodedJSValue regExpConstructorLeftContext(ExecState*, EncodedJSValue, PropertyName);
static EncodedJSValue regExpConstructorRightContext(ExecState*, EncodedJSValue, PropertyName);
static EncodedJSValue regExpConstructorDollar1(ExecState*, EncodedJSValue, PropertyName);
static EncodedJSValue regExpConstructorDollar2(ExecState*, EncodedJSValue, PropertyName);
static EncodedJSValue regExpConstructorDollar3(ExecState*, EncodedJSValue, PropertyName);
static EncodedJSValue regExpConstructorDollar4(ExecState*, EncodedJSValue, PropertyName);
static EncodedJSValue regExpConstructorDollar5(ExecState*, EncodedJSValue, PropertyName);
static EncodedJSValue regExpConstructorDollar6(ExecState*, EncodedJSValue, PropertyName);
static EncodedJSValue regExpConstructorDollar7(ExecState*, EncodedJSValue, PropertyName);
static EncodedJSValue regExpConstructorDollar8(ExecState*, EncodedJSValue, PropertyName);
static EncodedJSValue regExpConstructorDollar9(ExecState*, EncodedJSValue, PropertyName);

static bool setRegExpConstructorInput(ExecState*, EncodedJSValue, EncodedJSValue);
static bool setRegExpConstructorMultiline(ExecState*, EncodedJSValue, EncodedJSValue);

} // namespace JSC

#include "RegExpConstructor.lut.h"

namespace JSC {

const ClassInfo RegExpConstructor::s_info = { "Function", &InternalFunction::s_info, &regExpConstructorTable, CREATE_METHOD_TABLE(RegExpConstructor) };

/* Source for RegExpConstructor.lut.h
@begin regExpConstructorTable
    input           regExpConstructorInput          None
    $_              regExpConstructorInput          DontEnum
    multiline       regExpConstructorMultiline      None
    $*              regExpConstructorMultiline      DontEnum
    lastMatch       regExpConstructorLastMatch      DontDelete|ReadOnly
    $&              regExpConstructorLastMatch      DontDelete|ReadOnly|DontEnum
    lastParen       regExpConstructorLastParen      DontDelete|ReadOnly
    $+              regExpConstructorLastParen      DontDelete|ReadOnly|DontEnum
    leftContext     regExpConstructorLeftContext    DontDelete|ReadOnly
    $`              regExpConstructorLeftContext    DontDelete|ReadOnly|DontEnum
    rightContext    regExpConstructorRightContext   DontDelete|ReadOnly
    $'              regExpConstructorRightContext   DontDelete|ReadOnly|DontEnum
    $1              regExpConstructorDollar1        DontDelete|ReadOnly
    $2              regExpConstructorDollar2        DontDelete|ReadOnly
    $3              regExpConstructorDollar3        DontDelete|ReadOnly
    $4              regExpConstructorDollar4        DontDelete|ReadOnly
    $5              regExpConstructorDollar5        DontDelete|ReadOnly
    $6              regExpConstructorDollar6        DontDelete|ReadOnly
    $7              regExpConstructorDollar7        DontDelete|ReadOnly
    $8              regExpConstructorDollar8        DontDelete|ReadOnly
    $9              regExpConstructorDollar9        DontDelete|ReadOnly
@end
*/

RegExpConstructor::RegExpConstructor(VM& vm, Structure* structure, RegExpPrototype* regExpPrototype)
    : InternalFunction(vm, structure)
    , m_cachedResult(vm, this, regExpPrototype->emptyRegExp())
    , m_multiline(false)
{
}

void RegExpConstructor::finishCreation(VM& vm, RegExpPrototype* regExpPrototype, GetterSetter* speciesSymbol)
{
    Base::finishCreation(vm, ASCIILiteral("RegExp"));
    ASSERT(inherits(info()));

    putDirectWithoutTransition(vm, vm.propertyNames->prototype, regExpPrototype, DontEnum | DontDelete | ReadOnly);

    // no. of arguments for constructor
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(2), ReadOnly | DontDelete | DontEnum);

    putDirectNonIndexAccessor(vm, vm.propertyNames->speciesSymbol, speciesSymbol, Accessor | ReadOnly | DontEnum);
}

void RegExpConstructor::destroy(JSCell* cell)
{
    static_cast<RegExpConstructor*>(cell)->RegExpConstructor::~RegExpConstructor();
}

void RegExpConstructor::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    RegExpConstructor* thisObject = jsCast<RegExpConstructor*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    thisObject->m_cachedResult.visitChildren(visitor);
}

JSValue RegExpConstructor::getBackref(ExecState* exec, unsigned i)
{
    JSArray* array = m_cachedResult.lastResult(exec, this);

    if (i < array->length()) {
        JSValue result = JSValue(array).get(exec, i);
        ASSERT(result.isString() || result.isUndefined());
        if (!result.isUndefined())
            return result;
    }
    return jsEmptyString(exec);
}

JSValue RegExpConstructor::getLastParen(ExecState* exec)
{
    JSArray* array = m_cachedResult.lastResult(exec, this);
    unsigned length = array->length();
    if (length > 1) {
        JSValue result = JSValue(array).get(exec, length - 1);
        ASSERT(result.isString() || result.isUndefined());
        if (!result.isUndefined())
            return result;
    }
    return jsEmptyString(exec);
}

JSValue RegExpConstructor::getLeftContext(ExecState* exec)
{
    return m_cachedResult.leftContext(exec, this);
}

JSValue RegExpConstructor::getRightContext(ExecState* exec)
{
    return m_cachedResult.rightContext(exec, this);
}
    
bool RegExpConstructor::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<RegExpConstructor, InternalFunction>(exec, regExpConstructorTable, jsCast<RegExpConstructor*>(object), propertyName, slot);
}
    
EncodedJSValue regExpConstructorDollar1(ExecState* exec, EncodedJSValue thisValue, PropertyName)
{
    return JSValue::encode(asRegExpConstructor(JSValue::decode(thisValue))->getBackref(exec, 1));
}

EncodedJSValue regExpConstructorDollar2(ExecState* exec, EncodedJSValue thisValue, PropertyName)
{
    return JSValue::encode(asRegExpConstructor(JSValue::decode(thisValue))->getBackref(exec, 2));
}

EncodedJSValue regExpConstructorDollar3(ExecState* exec, EncodedJSValue thisValue, PropertyName)
{
    return JSValue::encode(asRegExpConstructor(JSValue::decode(thisValue))->getBackref(exec, 3));
}

EncodedJSValue regExpConstructorDollar4(ExecState* exec, EncodedJSValue thisValue, PropertyName)
{
    return JSValue::encode(asRegExpConstructor(JSValue::decode(thisValue))->getBackref(exec, 4));
}

EncodedJSValue regExpConstructorDollar5(ExecState* exec, EncodedJSValue thisValue, PropertyName)
{
    return JSValue::encode(asRegExpConstructor(JSValue::decode(thisValue))->getBackref(exec, 5));
}

EncodedJSValue regExpConstructorDollar6(ExecState* exec, EncodedJSValue thisValue, PropertyName)
{
    return JSValue::encode(asRegExpConstructor(JSValue::decode(thisValue))->getBackref(exec, 6));
}

EncodedJSValue regExpConstructorDollar7(ExecState* exec, EncodedJSValue thisValue, PropertyName)
{
    return JSValue::encode(asRegExpConstructor(JSValue::decode(thisValue))->getBackref(exec, 7));
}

EncodedJSValue regExpConstructorDollar8(ExecState* exec, EncodedJSValue thisValue, PropertyName)
{
    return JSValue::encode(asRegExpConstructor(JSValue::decode(thisValue))->getBackref(exec, 8));
}

EncodedJSValue regExpConstructorDollar9(ExecState* exec, EncodedJSValue thisValue, PropertyName)
{
    return JSValue::encode(asRegExpConstructor(JSValue::decode(thisValue))->getBackref(exec, 9));
}

EncodedJSValue regExpConstructorInput(ExecState*, EncodedJSValue thisValue, PropertyName)
{
    return JSValue::encode(asRegExpConstructor(JSValue::decode(thisValue))->input());
}

EncodedJSValue regExpConstructorMultiline(ExecState*, EncodedJSValue thisValue, PropertyName)
{
    return JSValue::encode(jsBoolean(asRegExpConstructor(JSValue::decode(thisValue))->multiline()));
}

EncodedJSValue regExpConstructorLastMatch(ExecState* exec, EncodedJSValue thisValue, PropertyName)
{
    return JSValue::encode(asRegExpConstructor(JSValue::decode(thisValue))->getBackref(exec, 0));
}

EncodedJSValue regExpConstructorLastParen(ExecState* exec, EncodedJSValue thisValue, PropertyName)
{
    return JSValue::encode(asRegExpConstructor(JSValue::decode(thisValue))->getLastParen(exec));
}

EncodedJSValue regExpConstructorLeftContext(ExecState* exec, EncodedJSValue thisValue, PropertyName)
{
    return JSValue::encode(asRegExpConstructor(JSValue::decode(thisValue))->getLeftContext(exec));
}

EncodedJSValue regExpConstructorRightContext(ExecState* exec, EncodedJSValue thisValue, PropertyName)
{
    return JSValue::encode(asRegExpConstructor(JSValue::decode(thisValue))->getRightContext(exec));
}

bool setRegExpConstructorInput(ExecState* exec, EncodedJSValue thisValue, EncodedJSValue value)
{
    if (auto constructor = jsDynamicCast<RegExpConstructor*>(JSValue::decode(thisValue))) {
        constructor->setInput(exec, JSValue::decode(value).toString(exec));
        return true;
    }
    return false;
}

bool setRegExpConstructorMultiline(ExecState* exec, EncodedJSValue thisValue, EncodedJSValue value)
{
    if (auto constructor = jsDynamicCast<RegExpConstructor*>(JSValue::decode(thisValue))) {
        constructor->setMultiline(JSValue::decode(value).toBoolean(exec));
        return true;
    }
    return false;
}

inline Structure* getRegExpStructure(ExecState* exec, JSGlobalObject* globalObject, JSValue newTarget)
{
    Structure* structure = globalObject->regExpStructure();
    if (newTarget != jsUndefined())
        structure = InternalFunction::createSubclassStructure(exec, newTarget, structure);
    return structure;
}

inline RegExpFlags toFlags(ExecState* exec, JSValue flags)
{
    if (flags.isUndefined())
        return NoFlags;
    JSString* flagsString = flags.toString(exec);
    if (!flagsString) {
        ASSERT(exec->hadException());
        return InvalidFlags;
    }

    RegExpFlags result = regExpFlags(flagsString->value(exec));
    if (exec->hadException())
        return InvalidFlags;
    if (result == InvalidFlags)
        throwSyntaxError(exec, ASCIILiteral("Invalid flags supplied to RegExp constructor."));
    return result;
}

static JSObject* regExpCreate(ExecState* exec, JSGlobalObject* globalObject, JSValue newTarget, JSValue patternArg, JSValue flagsArg)
{
    VM& vm = exec->vm();
    String pattern = patternArg.isUndefined() ? emptyString() : patternArg.toString(exec)->value(exec);
    if (exec->hadException())
        return nullptr;

    RegExpFlags flags = toFlags(exec, flagsArg);
    if (flags == InvalidFlags)
        return nullptr;

    RegExp* regExp = RegExp::create(vm, pattern, flags);
    if (!regExp->isValid())
        return vm.throwException(exec, createSyntaxError(exec, regExp->errorMessage()));

    Structure* structure = getRegExpStructure(exec, globalObject, newTarget);
    if (vm.exception())
        return nullptr;
    return RegExpObject::create(vm, structure, regExp);
}

JSObject* constructRegExp(ExecState* exec, JSGlobalObject* globalObject, const ArgList& args,  JSObject* callee, JSValue newTarget)
{
    VM& vm = exec->vm();
    JSValue patternArg = args.at(0);
    JSValue flagsArg = args.at(1);

    bool isPatternRegExp = patternArg.inherits(RegExpObject::info());
    bool constructAsRegexp = isRegExp(vm, exec, patternArg);

    if (newTarget.isUndefined() && constructAsRegexp && flagsArg.isUndefined()) {
        JSValue constructor = patternArg.get(exec, vm.propertyNames->constructor);
        if (vm.exception())
            return nullptr;
        if (callee == constructor) {
            // We know that patternArg is a object otherwise constructAsRegexp would be false.
            return patternArg.getObject();
        }
    }

    if (isPatternRegExp) {
        RegExp* regExp = jsCast<RegExpObject*>(patternArg)->regExp();
        Structure* structure = getRegExpStructure(exec, globalObject, newTarget);
        if (exec->hadException())
            return nullptr;

        if (!flagsArg.isUndefined()) {
            RegExpFlags flags = toFlags(exec, flagsArg);
            if (flags == InvalidFlags)
                return nullptr;
            regExp = RegExp::create(vm, regExp->pattern(), flags);
        }

        return RegExpObject::create(exec->vm(), structure, regExp);
    }

    if (constructAsRegexp) {
        JSValue pattern = patternArg.get(exec, vm.propertyNames->source);
        if (flagsArg.isUndefined())
            flagsArg = patternArg.get(exec, vm.propertyNames->flags);
        patternArg = pattern;
    }

    return regExpCreate(exec, globalObject, newTarget, patternArg, flagsArg);
}

EncodedJSValue JSC_HOST_CALL esSpecRegExpCreate(ExecState* exec)
{
    JSGlobalObject* globalObject = exec->lexicalGlobalObject();
    JSValue patternArg = exec->argument(0);
    JSValue flagsArg = exec->argument(1);
    return JSValue::encode(regExpCreate(exec, globalObject, jsUndefined(), patternArg, flagsArg));
}

static EncodedJSValue JSC_HOST_CALL constructWithRegExpConstructor(ExecState* exec)
{
    ArgList args(exec);
    return JSValue::encode(constructRegExp(exec, asInternalFunction(exec->callee())->globalObject(), args, exec->callee(), exec->newTarget()));
}

ConstructType RegExpConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructWithRegExpConstructor;
    return ConstructType::Host;
}

static EncodedJSValue JSC_HOST_CALL callRegExpConstructor(ExecState* exec)
{
    ArgList args(exec);
    return JSValue::encode(constructRegExp(exec, asInternalFunction(exec->callee())->globalObject(), args, exec->callee()));
}

CallType RegExpConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = callRegExpConstructor;
    return CallType::Host;
}

} // namespace JSC
