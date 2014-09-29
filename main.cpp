/*
 * main.cpp
 *
 *  Created on: Apr 9, 2014
 *      Author: Pimenta
 */

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <queue>
#include <string>
#include <sstream>

using namespace std;

typedef uint32_t uword_t;

#ifndef MEM_WORDS
#define MEM_WORDS 0x2000
#endif

struct AssemblyFile {
  fstream f;
  queue<string> pushed_tokens;
  int currentLine, lastTokenLine, currentTokenLine;
  
  AssemblyFile(const string& fn) :
  f(fn.c_str(), fstream::in),
  currentLine(1), lastTokenLine(1), currentTokenLine(1) {
    
  }
  
  string readToken() {
    string token = "";
    
    if (pushed_tokens.size()) {
      token = pushed_tokens.front();
      pushed_tokens.pop();
      return token;
    }
    
    lastTokenLine = currentTokenLine;
    
    for (char c = f.get(); f.good(); c = f.get()) {
      if (c == '/') { // comment found
        if (token.size()) { // token was read
          break;
        }
        // ignoring comment
        for (c = f.get(); c != '\r' && c != '\n' && f.good(); c = f.get());
        if (c == '\r') { // checking for CR or CRLF line endings
          c = f.get();
          if (f.good() && c != '\n') {
            f.unget();
          }
        }
        currentLine++;
      }
      else if (c == '\r' || c == '\n') { // line break found
        if (c == '\r') { // checking for CR or CRLF line endings
          c = f.get();
          if (f.good() && c != '\n') {
            f.unget();
          }
        }
        if (token.size()) { // token was read
          currentTokenLine = currentLine++;
          return token;
        }
        currentLine++;
      }
      else if (c == ' ' || c == '\t') { // white space found
        if (token.size()) { // token was read
          break;
        }
      }
      else { // concatenating the character read
        token += c;
      }
    }
    
    currentTokenLine = currentLine;
    return token;
  }
  
  AssemblyFile& push(const string& token) {
    pushed_tokens.push(token);
    return *this;
  }
  
  void close() {
    f.close();
  }
};

struct Field {
  uword_t word;
  bool is_hex;
  Field() : word(0), is_hex(false) {
    
  }
};

class ObjectCode {
private:
  set<string> pseudo_instructions;
  
  set<string> exported;
  map<string, uword_t> symbols;
  map<string, set<uword_t>> references;
  set<uword_t> relatives;
  uword_t mem_size = 0;
  uword_t* mem = new uword_t[MEM_WORDS];
  AssemblyFile af;
  string token;
public:
  ObjectCode(const string& fn) :
  mem_size(0), mem(new uword_t[MEM_WORDS]), af(fn)
  {
    initPseudoInstructions();
    readExportSection();
    readDataSection();
    readTextSection();
    af.close();
    resolveReferences();
  }
  
  ~ObjectCode() {
    delete[] mem;
  }
private:
  void initPseudoInstructions() {
    pseudo_instructions.insert("add");
    pseudo_instructions.insert("sub");
    pseudo_instructions.insert("neg");
    pseudo_instructions.insert("clr");
    pseudo_instructions.insert("mov");
    pseudo_instructions.insert("jmp");
    pseudo_instructions.insert("beq");
    pseudo_instructions.insert("bne");
    pseudo_instructions.insert("bge");
    pseudo_instructions.insert("ble");
    pseudo_instructions.insert("bgt");
    pseudo_instructions.insert("blt");
    pseudo_instructions.insert("bt");
    pseudo_instructions.insert("bf");
  }
  
  void readExportSection() {
    af.readToken(); // irgnore ".export" token
    for (token = af.readToken(); token != ".data"; token = af.readToken()) {
      exported.insert(token);
    }
  }
  
  void readDataSection() {
    // for pseudo-instructions
    af.push("$tmp:")  .push("0");
    af.push("$tmp2:") .push("0");
    
    for (token = af.readToken(); token != ".text";) {
      symbols[token.substr(0, token.size() - 1)] = mem_size;
      token = af.readToken();
      if (token == ".array") { // uninitialized array
        token = af.readToken();
        mem_size += parseData();
        token = af.readToken(); // next symbol
      }
      else if (token == ".iarray") { // initialized array
        for (
          token = af.readToken();
          af.currentTokenLine == af.lastTokenLine;
          token = af.readToken()
        ) {
          mem[mem_size++] = parseData();
        }
      }
      else if (token == ".ptr") { // pointer
        token = af.readToken();
        pushField();
        token = af.readToken(); // next symbol
      }
      else { // initialized word
        mem[mem_size++] = parseData();
        token = af.readToken(); // next symbol
      }
    }
  }
  
  void readTextSection() {
    int field_id = 0;
    for (token = af.readToken(); token.size();) {
      // field 2 omitted
      if (field_id == 2 && af.currentTokenLine != af.lastTokenLine) {
        relatives.emplace(mem_size);
        mem[mem_size] = mem_size + 1;
        mem_size++;
        field_id = (field_id + 1)%3;
      }
      // symbol found
      else if (token[token.size() - 1] == ':') {
        symbols[token.substr(0, token.size() - 1)] = mem_size;
        if (token == "start:")
          exported.emplace("start");
        token = af.readToken();
      }
      // pseudo-instructions
      else if (pseudo_instructions.find(token) != pseudo_instructions.end()) {
        if (token == "add") {
          readAdd();
        }
        else if (token == "sub") {
          readSub();
        }
        else if (token == "neg") {
          readNeg();
        }
        else if (token == "clr") {
          readClr();
        }
        else if (token == "mov") {
          readMov();
        }
        else if (token == "jmp") {
          readJmp();
        }
        else if (token == "beq") {
          readBeq();
        }
        else if (token == "bne") {
          readBne();
        }
        else if (token == "bge") {
          readBge();
        }
        else if (token == "ble") {
          readBle();
        }
        else if (token == "bgt") {
          readBgt();
        }
        else if (token == "blt") {
          readBlt();
        }
        else if (token == "bt") {
          readBt();
        }
        else if (token == "bf") {
          readBf();
        }
      }
      // field 0, 1, or field 2 specified
      else {
        pushField();
        field_id = (field_id + 1)%3;
        token = af.readToken();
      }
    }
  }
  
  void readAdd() {
    string a = af.readToken();
    string b = af.readToken();
    string c = af.readToken();
    if (af.currentTokenLine == af.lastTokenLine) { // add a b c (a = b + c;)
      af.push("$tmp") .push("$tmp");
      af.push(b)      .push("$tmp");
      af.push(c)      .push("$tmp");
      af.push(a)      .push(a);
      af.push("$tmp") .push(a);
      token = af.readToken();
    }
    else { // add a b (a += b;)
      af.push("$tmp") .push("$tmp");
      af.push(b)      .push("$tmp");
      af.push("$tmp") .push(a);
      if (token.size() == 0) { // no token left in the file after field b
        token = af.readToken();
      }
    }
  }
  
  void readSub() {
    string a = af.readToken();
    string b = af.readToken();
    string c = af.readToken();
    if (af.currentTokenLine == af.lastTokenLine) { // sub a b c (a = b - c;)
      af.push("$tmp") .push("$tmp");
      af.push(b)      .push("$tmp");
      af.push(a)      .push(a);
      af.push("$tmp") .push(a);
      af.push(c)      .push(a);
      token = af.readToken();
    }
    else { // sub a b (a -= b;)
      af.push(b).push(a);
      if (token.size() == 0) { // no token left in the file after field b
        token = af.readToken();
      }
    }
  }
  
  void readNeg() {
    //TODO
  }
  
  void readClr() { // a = 0;
    string a = af.readToken();
    af.push(a).push(a);
    token = af.readToken();
  }
  
  void readMov() { // a = b;
    string a = af.readToken();
    string b = af.readToken();
    af.push("$tmp") .push("$tmp");
    af.push(b)      .push("$tmp");
    af.push(a)      .push(a);
    af.push("$tmp") .push(a);
    token = af.readToken();
  }
  
  void readJmp() { // goto label;
    string label = af.readToken();
    af.push("$tmp").push("$tmp").push(label);
    token = af.readToken();
  }
  
  void readBeq() { // if (a == b) goto label;
    string a = af.readToken();
    string b = af.readToken();
    string label = af.readToken();
    //FIXME af.push("")
  }
  
  void readBne() { // if (a != b) goto label;
    //TODO
  }
  
  void readBge() { // if (a >= b) goto label;
    string a = af.readToken();
    string b = af.readToken();
    string label = af.readToken();
    af.push("$tmp")   .push("$tmp");
    af.push(b)        .push("$tmp");
    af.push("$tmp2")  .push("$tmp2");
    af.push("$tmp")   .push("$tmp2");
    af.push(a)        .push("$tmp2");
    token = af.readToken();
  }
  
  void readBle() { // if (a <= b) goto label;
    string a = af.readToken();
    string b = af.readToken();
    string label = af.readToken();
    af.push("$tmp")   .push("$tmp");
    af.push(a)        .push("$tmp");
    af.push("$tmp2")  .push("$tmp2");
    af.push("$tmp")   .push("$tmp2");
    af.push(b)        .push("$tmp2");
    token = af.readToken();
  }
  
  void readBgt() { // if (a > b) goto label;
    //TODO
  }
  
  void readBlt() { // if (a < b) goto label;
    //TODO
  }
  
  void readBt() { // if (a) goto label;
    //TODO
  }
  
  void readBf() { // if (!a) goto label;
    //TODO
  }
  
  uword_t parseData() {
    uword_t data = 0;
    if (token[0] == '0' && token[1] == 'x') { // for hex notation
      sscanf(token.c_str(), "%i", &data);
    }
    else { // for decimal notation
      stringstream ss;
      ss << token;
      ss >> data; 
    }
    return data;
  }
  
  Field parseField() {
    Field field;
    if (token[0] == '0' && token[1] == 'x') { // checking for hex notation
      sscanf(token.c_str(), "%i", &field.word);
      field.is_hex = true;
    }
    else if (token.find("+") != string::npos) { // check for array offset
      string offset = token.substr(token.find("+") + 1, token.size());
      token = token.substr(0, token.find("+")); // remove offset from token
      stringstream ss;
      ss << offset;
      ss >> field.word;
    }
    return field;
  }
  
  void pushField() {
    Field field = parseField();
    if (!field.is_hex) {
      relatives.emplace(mem_size);
      auto sym = symbols.find(token);
      if (sym == symbols.end()) { // symbol not found. leave a reference
        references[token].emplace(mem_size);
      }
      else { // symbol found. the field is the address of the symbol
        field.word += sym->second;
      }
    }
    mem[mem_size++] = field.word;
  }
  
  void resolveReferences() {
    for (auto map_it = references.begin(); map_it != references.end();) {
      // external symbols
      auto sym = symbols.find(map_it->first);
      if (sym == symbols.end()) {
        ++map_it;
        continue;
      }
      
      // resolve
      for (auto it = map_it->second.begin(); it != map_it->second.end(); ++it) {
        mem[*it] += sym->second;
      }
      
      references.erase(map_it++);
    }
  }
public:
  void write(const string& fn) {
    fstream of(fn.c_str(), fstream::out | fstream::binary);
    uword_t tmp;
    
    // write number of exported symbols
    tmp = exported.size();
    of.write((const char*)&tmp, sizeof(uword_t));
    
    // write exported symbols
    for (auto& exp : exported) {
      // string
      of.write(exp.c_str(), exp.size() + 1);
      
      // address
      tmp = symbols[exp];
      of.write((const char*)&tmp, sizeof(uword_t));
    }
    
    // write number of symbols of pending references
    tmp = references.size();
    of.write((const char*)&tmp, sizeof(uword_t));
    
    // write symbols of pending references
    for (auto& sym : references) {
      // string
      of.write(sym.first.c_str(), sym.first.size() + 1);
      
      // write number of references to current symbol
      tmp = sym.second.size();
      of.write((const char*)&tmp, sizeof(uword_t));
      
      // write references to current symbol
      for (auto ref : sym.second) {
        tmp = ref;
        of.write((const char*)&tmp, sizeof(uword_t));
      }
    }
    
    // write number of relative addresses
    tmp = relatives.size();
    of.write((const char*)&tmp, sizeof(uword_t));
    
    // write relative addresses
    for (auto addr : relatives) {
      tmp = addr;
      of.write((const char*)&tmp, sizeof(uword_t));
    }
    
    // write assembled code size
    of.write((const char*)&mem_size, sizeof(uword_t));
    
    // write assembled code
    of.write((const char*)mem, sizeof(uword_t)*mem_size);
    
    of.close();
  }
};

int main(int argc, char* argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage mode: subleq-asm <assembly_file> <object_file>\n");
    return 0;
  }
  
  ObjectCode oc(argv[1]);
  oc.write(argv[2]);
  
  return 0;
}
