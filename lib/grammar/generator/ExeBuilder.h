//
// Created by BNunnally on 5/15/2020.
//

#ifndef SHARP_EXEBUILDER_H
#define SHARP_EXEBUILDER_H

class Compiler;
class ClassObject;
class Method;
class ExeBuilder {
public:
    ExeBuilder(Compiler *compiler)
    :
        compiler(compiler)
    {
        allMethods.init();
        allClasses.init();
        functionPointers.init();
    }

    ~ExeBuilder() {
        allMethods.free();
        allClasses.free();
        functionPointers.free();
    }

    void build();

private:
    List<Method*> allMethods;
    List<ClassObject*> allClasses;
    List<Method*> functionPointers;
    Compiler *compiler;
    stringstream buf;
    stringstream dataSec;

    void createDumpFile();
    void buildExe();
    void dumpClassInfo(ClassObject*);
    void addClass(ClassObject*);
    string registerToString(int64_t r);
    string codeToString(Method*);
    string getNote(Meta &meta);

    void buildHeader();

    void buildManifest();

    void buildSymbolSection();

    void buildFieldData(Field *field);
    void buildFieldData(ClassObject *pObject);
    void buildMethodData(ClassObject *pObject);
    void buildInterfaceData(ClassObject *pObject);

    void buildStringSection();
    void buildConstantSection();
    void addFunctionPointers();

    void buildDataSection();
    void putSymbol(Utype *utype, stringstream &buf);

    void putMethodData(Method *fun);

    void putMethodCode(Method *fun);

    void buildMetaDataSection();

    int32_t getFpOffset(Method *fun);

    int32_t getSpOffset(Method *fun);
    int32_t getSecondarySpOffset(Method *fun);
};


#endif //SHARP_EXEBUILDER_H
