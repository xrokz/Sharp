//
// Created by BraxtonN on 2/1/2018.
//

#ifndef SHARP_CLASSOBJECT_H
#define SHARP_CLASSOBJECT_H

#include <list>
#include "../../stdimports.h"
#include "Method.h"
#include "Field.h"
#include "AccessModifier.h"
#include "OperatorOverload.h"
#include "../util/KeyPair.h"

class Expression;
class Ast;

class ClassObject {

public:
    ClassObject()
            :
            name(""),
            module_name(""),
            serial(0),
            modifier(),
            base(NULL),
            super(NULL),
            head(NULL),
            note(),
            fullName(""),
            address(-1),
            _interface(false),
            _generic(false),
            processed(false),
            enumValue(0),
            _enum(false)
    {
        functions.init();
        constructors.init();
        overloads.init();
        fields.init();
        childClasses.init();
        genericKeys.init();
    }
    ClassObject(string name, string pmodule, long uid, AccessModifier modifier, RuntimeNote note)
            :
            name(name),
            module_name(pmodule),
            serial(uid),
            modifier(modifier),
            base(NULL),
            super(NULL),
            head(NULL),
            note(note),
            fullName(""),
            address(-1),
            _interface(false),
            _generic(false),
            processed(false),
            enumValue(0),
            _enum(false)
    {
        functions.init();
        constructors.init();
        overloads.init();
        fields.init();
        childClasses.init();
        genericKeys.init();
    }

    ClassObject(string name, string pmodule, long uid, AccessModifier modifier, RuntimeNote note,
                ClassObject* parent)
            :
            name(name),
            module_name(pmodule),
            serial(uid),
            modifier(modifier),
            base(NULL),
            head(NULL),
            super(parent),
            note(note),
            fullName(""),
            address(-1),
            _interface(false),
            _generic(false),
            processed(false),
            enumValue(0),
            _enum(false)
    {
        functions.init();
        constructors.init();
        overloads.init();
        fields.init();
        childClasses.init();
        genericKeys.init();
    }

    AccessModifier getAccessModifier() { return modifier; }
    long getSerial() { return serial; }
    void setAddress(long addr) { this->address=addr; }
    string getName() { return name; }
    string getModuleName() { return module_name; }
    ClassObject* getSuperClass() { return super; }
    ClassObject* getBaseClass() { return base; }
    ClassObject* getHeadClass() { return head; }
    bool match(ClassObject* klass) {
        return klass != NULL && klass->serial == serial;
    }

    bool assignable(ClassObject *klass, bool cast = false) {
        if(klass != NULL) {
            if(_interface && klass->hasInterface(this))
                return true;
            return klass->serial == serial || klass->hasBaseClass(this)
                   || (cast && this->hasInterface(klass));
        }
        return false;
    }
    void setBaseClass(ClassObject* base) {
        this->base = base;
    }
    void setSuperClass(ClassObject* sup) {
        this->super = sup;
    }
    void setHead(ClassObject* sup) {
        this->head = sup;
    }
    void setFullName(const string fullName) {
        this->fullName = fullName;
    }
    void setName(const string name) {
        this->name = name;
    }
    string getFullName() {
        return fullName;
    }

    void operator=(ClassObject& klass) {
        free();
        this->base = klass.base;
        this->childClasses.addAll(klass.childClasses);
        for(long i = 0; i < klass.constructors.size(); i++) {
            this->constructors.add(new Method());
            *this->constructors.last() = *klass.getConstructor(i);
        }
        for(long i = 0; i < klass.fields.size(); i++) {
            this->fields.add(new Field());
            *this->fields.last() = *klass.getField(i);
        }
        for(long i = 0; i < klass.functions.size(); i++) {
            this->functions.add(new Method());
            *this->functions.last() = *klass.getFunction(i);
        }
        for(long i = 0; i < klass.overloads.size(); i++) {
            this->overloads.add(new OperatorOverload());
            *this->overloads.last() = *klass.getOverload(i);
        }
        this->fullName = klass.fullName;
        this->head = klass.head;
        this->modifier = klass.modifier;
        this->module_name = klass.module_name;
        this->name = klass.name;
        this->note = klass.note;
        this->super = klass.super;
        this->serial = klass.serial;
        this->address = klass.address;
        this->_interface=klass._interface;
        this->_generic=klass._generic;
        this->genericKeys.addAll(klass.genericKeys);
        this->processed=klass.processed;
        this->start = klass.start;
        this->enumValue=klass.enumValue;
        this->_enum=klass._enum;
    }

    size_t constructorCount();
    Method* getConstructor(int p);
    Method* getConstructor(List<Param>& params, bool useBase =false, bool nativeSupport = false, bool ambiguousProtect = false, bool find = true);
    bool addConstructor(Method constr);

    size_t functionCount(bool ignore=false);
    Method* getFunction(int p);
    Method* getFunctionByName(string name, bool &ambiguous);
    Method* getFunction(string name, List<Param>& params, bool useBase =false, bool nativeSupport = false, bool skipdelegates=false, bool ambiguousProtect = false, bool find = true);
    Method* getFunction(string name, int64_t _offset);
    bool addFunction(Method function);

    size_t overloadCount();
    OperatorOverload* getOverload(size_t p);
    OperatorOverload* getPostIncOverload();
    OperatorOverload* getPostDecOverload();
    OperatorOverload* getPreIncOverload();
    OperatorOverload* getPreDecOverload();
    OperatorOverload* getOverload(Operator op, List<Param>& params, bool useBase =false, bool  = false, bool ambiguousProtect = false, bool find = true);
    OperatorOverload* getOverload(Operator op, int64_t _offset);
    bool hasOverload(Operator op);
    bool addOperatorOverload(OperatorOverload overload);

    size_t fieldCount();
    Field* getField(int p);
    Field* getField(string name, bool ubase =false);
    bool addField(Field field);

    size_t childClassCount();
    ClassObject* getChildClass(int p);
    ClassObject* getChildClass(string name);
    bool addChildClass(ClassObject *constr);
    void free();

    bool isInterface() { return _interface; }
    void setIsInterface(bool _interface) { this->_interface=_interface; }
    bool isGeneric() { return _generic; }
    bool isEnum() { return _enum; }
    bool isProcessed() { return processed; }
    bool addGenericType(Expression* utype);
    Expression* getGenericType(string &key);
    void setIsProcessed(bool proccessed) { this->processed = proccessed; };
    void setIsGeneric(bool _generic) { this->_generic=_generic; }
    void setIsEnum(bool _enum) { this->_enum=_enum; }
    void setAst(Ast* start) { this->start=start; }
    Ast* getAst() { return start; }
    void addGenericKey(string key) { this->genericKeys.push_back(key); }
    bool hasGenericKey(string key) {
        if(!this->genericKeys.find(key)) {
            if(super != NULL) {
                return super->hasGenericKey(key);
            } else return false;
        } else return true;
    }
    long genericKeySize() { return this->genericKeys.size(); }
    size_t interfaceCount() { return interfaces.size(); }
    ClassObject* getInterface(size_t p) { return interfaces.get(p); }
    bool duplicateInterface(ClassObject *intf) {
        int copys = 0;
        for(long i = 0; i < interfaces.size(); i++) {
            if(interfaces.get(i)==intf)
                copys++;
        }
        return copys > 1;
    }
    bool hasInterface(ClassObject *intf) {
        for(long i = 0; i < interfaces.size(); i++) {
            if(interfaces.get(i)==intf)
                return true;
        }
        if(base != NULL)
            return base->hasInterface(intf);
        return false;
    }
    void setInterfaces(List<ClassObject*> interfaces) { this->interfaces.addAll(interfaces); }
    Method *getDelegateFunction(string name, List<Param> &params, bool useBase = true, bool nativeSupport = true);

    RuntimeNote note;

    bool isCurcular(ClassObject *pObject);

    bool matchBase(ClassObject *pObject);

    bool hasBaseClass(ClassObject *pObject);

    long getFieldIndex(string name);

    int baseClassDepth(ClassObject *pObject);

    long getTotalFieldCount();

    long getTotalFunctionCount();
    long getFieldAddress(Field* field);

    long long address;
    long enumValue;

    List<Method *> getDelegatePosts(bool ubase);

    List<Method *> getDelegates();

private:
    AccessModifier modifier;
    long serial;
    Ast* start; // for parsing our generic class later
    string name;
    bool _interface, _generic, _enum;
    string fullName;
    string module_name;
    bool processed;
    List<string> genericKeys;
    List<Expression> genericMap;
    List<Method*> constructors;
    List<Method*> functions;
    List<OperatorOverload*> overloads;
    List<Field*> fields;
    List<ClassObject*> childClasses;
    List<ClassObject*> interfaces;
    ClassObject *super, *base, *head;

    Method *getDelegatePost(string name, List<Param> &params, bool useBase, bool nativeSupport, bool find = true);
};

#define totalFucntionCount(x) x->functionCount()+x->constructorCount()+x->overloadCount()


#endif //SHARP_CLASSOBJECT_H
