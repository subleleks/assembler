/*
 * assembler32.cpp
 *
 *  Created on: Jul 23, 2014
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

static fstream f;
static string buf;
static set<string> exported;
static map<string, uword_t> symbols;
static map<string, set<uword_t>> references;
static set<uword_t> relatives;
static uword_t mem_size = 0;
static uword_t* mem = new uword_t[MEM_WORDS];
static int currentLine = 1, lastTokenLine = 1, currentTokenLine = 1;

inline static void readToken() {
  buf = "";
  lastTokenLine = currentTokenLine;
  
  for (char c = f.get(); f.good(); c = f.get()) {
    if (c == '/') { // comment found
      if (buf.size()) { // token was read
        currentTokenLine = currentLine;
        return;
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
      if (buf.size()) { // token was read
        currentTokenLine = currentLine++;
        return;
      }
      currentLine++;
    }
    else if (c == ' ' || c == '\t') { // white space found
      if (buf.size()) { // token was read
        currentTokenLine = currentLine;
        return;
      }
    }
    else { // concatenating the character read
      buf += c;
    }
  }
  
  currentTokenLine = currentLine;
}

inline static uword_t parseData() {
  uword_t data = 0;
  if (buf[0] == '0' && buf[1] == 'x') { // for hex notation
    sscanf(buf.c_str(), "%i", &data);
  }
  else { // for decimal notation
    stringstream ss;
    ss << buf;
    ss >> data; 
  }
  return data;
}

inline static uword_t parseField() {
  uword_t field = 0;
  if (buf[0] == '0' && buf[1] == 'x') { // hex notation means absolute address
    sscanf(buf.c_str(), "%i", &field);
  }
  else { // symbol means an address that needs to be relocated later
    relatives.emplace(mem_size);
    
    // looking for array offset
    if (buf.find("+") != buf.npos) {
      string offset = buf.substr(buf.find("+") + 1, buf.size());
      buf = buf.substr(0, buf.find("+"));
      stringstream ss;
      ss << offset;
      ss >> field;
    }
    
    auto sym = symbols.find(buf);
    if (sym == symbols.end()) { // symbol not found. leave a reference
      references[buf].emplace(mem_size);
    }
    else { // symbol found. the field is the address of the symbol
      field = sym->second;
    }
  }
  return field;
}

int assembler32(int argc, char* argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage mode: subleq-asm <assembly_file> <object_file>\n");
    return 0;
  }
  
  f.open(argv[1]);
  
  readToken(); // reading ".export"
  
  // reading export section
  for (readToken(); buf != ".data"; readToken()) {
    exported.insert(buf);
  }
  
  // reading data section
  for (readToken(); buf != ".text";) {
    symbols[buf.substr(0, buf.size() - 1)] = mem_size;
    readToken();
    if (buf == ".array") { // uninitialized array
      readToken();
      mem_size += parseData();
      readToken(); // next symbol
    }
    else if (buf == ".iarray") { // initialized array
      for (readToken(); currentTokenLine == lastTokenLine; readToken()) {
        mem[mem_size++] = parseData();
      }
    }
    else if (buf == ".ptr") { // pointer
      mem[mem_size] = parseField();
      mem_size++;
      readToken(); // next symbol
    }
    else { // initialized word
      mem[mem_size++] = parseData();
      readToken(); // next symbol
    }
  }
  
  // reading text section
  int field = 0;
  for (readToken(); buf.size();) {
    // field 2 omitted
    if (field == 2 && currentTokenLine != lastTokenLine) {
      relatives.emplace(mem_size);
      mem[mem_size] = mem_size + 1;
      mem_size++;
      field = (field + 1)%3;
    }
    // symbol found
    else if (buf[buf.size() - 1] == ':') {
      symbols[buf.substr(0, buf.size() - 1)] = mem_size;
      if (buf == "start:")
        exported.emplace("start");
      readToken();
    }
    // field 0, 1, or field 2 specified
    else {
      mem[mem_size] = parseField();
      mem_size++;
      field = (field + 1)%3;
      readToken();
    }
  }
  
  f.close();
  
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
  
  f.open(argv[2], fstream::out | fstream::binary);
  
  {
    uword_t tmp;
    
    // write number of exported symbols
    tmp = exported.size();
    f.write((const char*)&tmp, sizeof(uword_t));
    
    // write exported symbols
    for (auto& exp : exported) {
      // string
      f.write(exp.c_str(), exp.size() + 1);
      
      // address
      tmp = symbols[exp];
      f.write((const char*)&tmp, sizeof(uword_t));
    }
    
    // write number of symbols of pending references
    tmp = references.size();
    f.write((const char*)&tmp, sizeof(uword_t));
    
    // write symbols of pending references
    for (auto& sym : references) {
      // string
      f.write(sym.first.c_str(), sym.first.size() + 1);
      
      // write number of references to current symbol
      tmp = sym.second.size();
      f.write((const char*)&tmp, sizeof(uword_t));
      
      // write references to current symbol
      for (auto ref : sym.second) {
        tmp = ref;
        f.write((const char*)&tmp, sizeof(uword_t));
      }
    }
    
    // write number of relative addresses
    tmp = relatives.size();
    f.write((const char*)&tmp, sizeof(uword_t));
    
    // write relative addresses
    for (auto addr : relatives) {
      tmp = addr;
      f.write((const char*)&tmp, sizeof(uword_t));
    }
    
    // write assembled code size
    f.write((const char*)&mem_size, sizeof(uword_t));
    
    // write assembled code
    f.write((const char*)mem, sizeof(uword_t)*mem_size);
  }
  
  f.close();
  
  delete[] mem;
  
  return 0;
}
