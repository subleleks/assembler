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
#include <string>
#include <sstream>

using namespace std;

typedef uint32_t uword_t;

#ifndef MEM_WORDS
#define MEM_WORDS 0x2000
#endif

struct AssemblyFile {
  fstream f;
  int currentLine, lastTokenLine, currentTokenLine;
  AssemblyFile(const string& fn) :
  f(fn.c_str(), fstream::in),
  currentLine(1), lastTokenLine(1), currentTokenLine(1) {
    
  }
  string readToken() {
    string token = "";
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
          return token;
        }
      }
      else { // concatenating the character read
        token += c;
      }
    }
    
    currentTokenLine = currentLine;
    return token;
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

inline static uword_t parseData(const string& token) {
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

inline static Field parseField(string& token) {
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

struct ObjectCode {
  set<string> exported;
  map<string, uword_t> symbols;
  map<string, set<uword_t>> references;
  set<uword_t> relatives;
  uword_t mem_size = 0;
  uword_t* mem = new uword_t[MEM_WORDS];
  ObjectCode(const string& fn) : mem_size(0), mem(new uword_t[MEM_WORDS]) {
    AssemblyFile af(fn);
    
    af.readToken(); // reading ".export"
    
    // reading export section
    for (
      string token = af.readToken();
      token != ".data";
      token = af.readToken()
    ) {
      exported.insert(token);
    }
    
    // reading data section
    for (string token = af.readToken(); token != ".text";) {
      symbols[token.substr(0, token.size() - 1)] = mem_size;
      token = af.readToken();
      if (token == ".array") { // uninitialized array
        token = af.readToken();
        mem_size += parseData(token);
        token = af.readToken(); // next symbol
      }
      else if (token == ".iarray") { // initialized array
        for (
          token = af.readToken();
          af.currentTokenLine == af.lastTokenLine;
          token = af.readToken()
        ) {
          mem[mem_size++] = parseData(token);
        }
      }
      else if (token == ".ptr") { // pointer
        token = af.readToken();
        Field field = parseField(token);
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
        mem[mem_size] = field.word;
        mem_size++;
        token = af.readToken(); // next symbol
      }
      else { // initialized word
        mem[mem_size++] = parseData(token);
        token = af.readToken(); // next symbol
      }
    }
    
    // reading text section
    int field_id = 0;
    for (string token = af.readToken(); token.size();) {
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
      // field 0, 1, or field 2 specified
      else {
        Field field = parseField(token);
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
        mem[mem_size] = field.word;
        mem_size++;
        field_id = (field_id + 1)%3;
        token = af.readToken();
      }
    }
    
    af.close();
    
    // resolve references
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
  
  ~ObjectCode() {
    delete[] mem;
  }
  
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
