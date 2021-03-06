//
// Created by BraxtonN on 2/2/2018.
//

#ifndef SHARP_FIELD_H
#define SHARP_FIELD_H

#include "../../stdimports.h"
#include "FieldType.h"
#include "AccessModifier.h"
#include "RuntimeNote.h"
#include "List.h"

class ClassObject;
class Ast;
class Method;
class Param;
class Expression;

class Field {
public:
    Field(FieldType type, uint64_t serial, const string name, ClassObject* parent, List<AccessModifier>& modifiers,
          RuntimeNote& note, StorageLocality stl, int64_t taddr)
            :
            type(type),
            serial(serial),
            name(name),
            fullName(""),
            owner(parent),
            klass(NULL),
            note(note),
            isArray(false),
            nullType(false),
            local(false),
            key(""),
            ast(NULL),
            proto(NULL),
            isEnum(false),
            constant_value(0),
            prototype(false),
            returnType(TYPEVOID),
            locality(stl),
            thread_address(taddr),
            defaultValue(false),
            defValExpr(NULL)
    {
        this->modifiers.init();
        this->params.init();
        this->modifiers.addAll(modifiers);
    }

    Field(ClassObject* klass, uint64_t serial, const string name, ClassObject* parent, List<AccessModifier>& modifiers,
          RuntimeNote& note, StorageLocality stl, int64_t taddr)
            :
            type(UNDEFINED),
            serial(serial),
            name(name),
            fullName(""),
            owner(parent),
            klass(klass),
            note(note),
            isArray(false),
            nullType(false),
            local(false),
            key(""),
            ast(NULL),
            proto(NULL),
            isEnum(false),
            constant_value(0),
            prototype(false),
            returnType(TYPEVOID),
            locality(stl),
            thread_address(taddr),
            defaultValue(false),
            defValExpr(NULL)
    {
        this->modifiers.init();
        this->params.init();
        this->modifiers.addAll(modifiers);
    }

    Field()
            :
            type(UNDEFINED),
            serial(0),
            name(""),
            fullName(""),
            modifiers(),
            note("","",0,0),
            isArray(false),
            nullType(false),
            local(false),
            owner(NULL),
            key(""),
            klass(NULL),
            ast(NULL),
            proto(NULL),
            isEnum(false),
            constant_value(0),
            prototype(false),
            returnType(TYPEVOID),
            locality(stl_local),
            thread_address(0),
            defaultValue(false),
            defValExpr(NULL)
    {
    }

    bool operator==(Field& f);

    void operator=(Field f);

    void free();

    void init() {

        type = UNDEFINED;
        serial = 0;
        name = "";
        fullName = "";
        modifiers.init();
        note = RuntimeNote("","",0,0);
        isArray = false;
        nullType = false;
        local = false;
        owner = NULL;
        key = "";
        klass = NULL;
        ast = NULL;
        proto = NULL;
        isEnum = false;
        constant_value = 0;
        prototype = false;
        returnType = TYPEVOID;
        locality = stl_local;
        thread_address = 0;
        defaultValue = false;
        defValExpr = NULL;
    }

//    bool isField() {
//        return nf != fnof || nf >= fdynamic;
//    }

    bool isStatic() {
        for(unsigned int i = 0; i < modifiers.size(); i++) {
            if(modifiers.at(i) == STATIC)
                return true;
        }
        return false;
    }

    bool isConst() {
        for(unsigned int i = 0; i < modifiers.size(); i++) {
            if(modifiers.at(i) == mCONST)
                return true;
        }
        return false;
    }


    bool isVar() {
        return type == VAR;
    }

    bool isNative() {
        return isVar() || dynamicObject();
    }

    bool isObjectInMemory() {
        return type == CLASS || isArray || dynamicObject();
    }

    bool dynamicObject() {
        return type == OBJECT;
    }

    bool hasThreadLocality() {
        return locality == stl_thread;
    }

    List<Param>& getParams();

    bool isArray, nullType, local, isEnum;
    bool resolved;
    bool prototype;
    bool defaultValue;
    Method* proto;
    FieldType returnType;
    double constant_value;
    RuntimeNote note;
    Expression* defValExpr;
    FieldType type;
    ClassObject* klass;
    List<Param> params; // for prototypes
    Ast *ast; // for parsing a generic field later
    int64_t serial, address;
    string name, fullName;
    ClassObject* owner;
    string key; // generic identifier
    List<AccessModifier> modifiers; // 3 max modifiers
    StorageLocality locality;
    int64_t thread_address;
};

#endif //SHARP_FIELD_H
