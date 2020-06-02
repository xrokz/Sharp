//
// Created by BraxtonN on 10/22/2019.
//

#include "ClassObject.h"
#include "../Compiler.h"
#include "../data/Alias.h"


void ClassObject::free()
{
    release();
    keys.free();
    Compiler::freeListPtr(fields);
    Compiler::freeListPtr(classes);
    Compiler::freeListPtr(keyTypes);
    Compiler::freeListPtr(functions);
}

Field* ClassObject::getField(string name, bool checkBase) {
    Field *field = NULL;
    for(long i = 0; i < fields.size(); i++) {
        field = fields.get(i);
        if(field->name == name) {
            return field;
        }
    }

    if(checkBase && super)
        return super->getField(name, true);

    return NULL;
}

Alias* ClassObject::getAlias(string name, bool checkBase) {
    Alias *alias = NULL;
    for(long i = 0; i < aliases.size(); i++) {
        alias = aliases.get(i);
        if(alias->name == name) {
            return alias;
        }
    }

    if(checkBase && super)
        return super->getAlias(name, true);

    return NULL;
}

long ClassObject::getFieldIndex(string &name) {
    long iter = 0;
    for(unsigned int i = 0; i < fields.size(); i++) {
        if(fields.get(i)->name == name)
            return iter;
        iter++;
    }

    return iter;
}

long ClassObject::getFieldAddress(Field* field) {
    bool isStatic = field->flags.find(STATIC);
    if(super == NULL) {
        if(isStatic)
            return getStaticFieldAddress(field->name);
        else
            return getInstanceFieldAddress(field->name);
    }

    ClassObject* k, *_klass = this;
    long address=0;

    for(;;) {
        k = _klass->getSuperClass();

        if(k == NULL) {
            if(isStatic)
                return address + getStaticFieldAddress(field->name);
            else return address + getInstanceFieldAddress(field->name);
        }

        if(isStatic)
            address+= k->getStaticFieldCount();
        else address += k->getInstanceFieldCount();
        _klass = k;
    }
}

long ClassObject::totalFieldCount() {
    ClassObject* k, *_klass = this;
    long fieldCount=fields.size();

    for(;;) {
        k = _klass->getSuperClass();

        if(k == NULL)
            return fieldCount;

        fieldCount+=k->fields.size();
        _klass = k;
    }
}

long ClassObject::totalInstanceFieldCount() {
    ClassObject* k, *_klass = this;
    long fieldCount=getInstanceFieldCount();

    for(;;) {
        k = _klass->getSuperClass();

        if(k == NULL)
            return fieldCount;

        fieldCount+=k->getInstanceFieldCount();
        _klass = k;
    }
}

long ClassObject::totalStaticFieldCount() {
    ClassObject* k, *_klass = this;
    long fieldCount=getStaticFieldCount();

    for(;;) {
        k = _klass->getSuperClass();

        if(k == NULL)
            return fieldCount;

        fieldCount+=k->getStaticFieldCount();
        _klass = k;
    }
}

long ClassObject::totalFunctionCount() {
    ClassObject* k, *_klass = this;
    long functionCount=functions.size();

    for(;;) {
        k = _klass->getSuperClass();

        if(k == NULL)
            return functionCount;

        functionCount+=k->functions.size();
        _klass = k;
    }
}

long ClassObject::totalInterfaceCount() {
    ClassObject* k, *_klass = this;
    long InterfaceCount=interfaces.size();

    for(;;) {
        k = _klass->getSuperClass();

        if(k == NULL)
            return InterfaceCount;

        InterfaceCount+=k->interfaces.size();
        _klass = k;
    }
}

bool ClassObject::isClassRelated(ClassObject *klass, bool interfaceCheck) {
    if(klass == NULL) return false;
    if(match(klass)) return true;

    if(interfaceCheck) {
        for (long long i = 0; i < interfaces.size(); i++) {
            if (interfaces.get(i)->isClassRelated(klass, false))
                return true;
        }
    }

    return super == NULL ? false : super->isClassRelated(klass, interfaceCheck);
}

bool ClassObject::getAllFunctionsByName(string name, List<Method*> &funcs, bool checkBase) {
    for(size_t i = 0; i < functions.size(); i++) {
        if(name == functions.get(i)->name) {
            funcs.add(functions.get(i));
        }
    }
    if(checkBase && super)
        return super->getAllFunctionsByName(name, funcs, true);
    return !funcs.empty();
}

size_t ClassObject::fieldCount() {
    return fields.size();
}

Field *ClassObject::getField(long index) {
    return fields.get(index);
}

Method* ClassObject::getFunction(long index) {
    return functions.get(index);
}

