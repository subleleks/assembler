/*
 * assembler64.cpp
 *
 *  Created on: Jul 23, 2014
 *      Author: Pimenta
 */

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <list>
#include <map>
#include <set>
#include <string>
#include <sstream>

using namespace std;

typedef int64_t  word_t;
typedef uint64_t uword_t;
typedef uint32_t address_t;

enum field_t {
  A, B, J
};

#ifndef MEM_WORDS
#define MEM_WORDS 0x2000
#endif

static const uword_t ADDRESS_WIDTH = uword_t(log2(double(MEM_WORDS)));

static fstream f;
static string buf;
static address_t text_offset;
static set<string> exported;
static map<string, address_t> symbols;
static map<string, list<pair<address_t, field_t>>> references;
static list<pair<address_t, field_t>> absolute;
static address_t mem_size = 0;
static uword_t* mem = new uword_t[MEM_WORDS];

inline static void readField(uword_t& instr, field_t field) {
  int mult = field == A ? 2 : (field == B ? 1 : 0);
  if (buf[0] == '0' && buf[1] == 'x') {
    address_t tmp;
    sscanf(buf.c_str(), "%i", &tmp);
    instr |= (tmp << mult*ADDRESS_WIDTH);
    absolute.emplace_back(mem_size, field);
  }
  else {
    auto sym = symbols.find(buf);
    if (sym == symbols.end())
      references[buf].emplace_back(mem_size, field);
    else
      instr |= (uword_t(sym->second) << mult*ADDRESS_WIDTH);
  }
}

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

int assembler64(int argc, char* argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage mode: subleq-asm <assembly_file> <object_file>\n");
    return 0;
  }
  
  f.open(argv[1]);
  
  readToken(); // reading ".export"
  
  for (readToken(); buf != ".data"; readToken()) { // reading export section
    exported.insert(buf);
  }
  
  for (readToken(); buf != ".text"; readToken()) { // reading data section
    symbols[buf.substr(0, buf.size() - 1)] = mem_size;
    readToken();
    if (buf[0] == '0' && buf[1] == 'x') { // for hex notation
      sscanf(buf.c_str(), "%lli", &mem[mem_size++]);
    }
    else { // for decimal notation
      stringstream ss;
      ss << buf;
      ss >> mem[mem_size++]; 
    }
  }
  
  text_offset = mem_size;
  for (readToken(); buf.size();) { // reading text section
    if (buf[buf.size() - 1] == ':') { // label found
      symbols[buf.substr(0, buf.size() - 1)] = mem_size;
      
      if (buf == "start:") // exporting start label
        exported.insert(string("start"));
      
      readToken();
      continue;
    }
    
    uword_t instr = 0;
    readField(instr, A);
    readToken();
    readField(instr, B);
    
    // reading field J
    readToken();
    if (currentTokenLine != lastTokenLine || !buf.size()) { // field omitted
      instr |= uword_t(mem_size + 1);
      mem[mem_size++] = instr;
      continue;
    }
    else { // field specified
      readField(instr, J);
    }
    
    readToken();
    mem[mem_size++] = instr;
  }
  
  f.close();
  
  // solve references
  for (auto map_it = references.begin(); map_it != references.end();) {
    // external symbols
    auto sym = symbols.find(map_it->first);
    if (sym == symbols.end()) {
      ++map_it;
      continue;
    }
    
    // solve
    for (auto it = map_it->second.begin(); it != map_it->second.end(); ++it) {
      uword_t& instr = mem[it->first];
      switch (it->second) {
        case A:
          instr |= (uword_t(sym->second) << 2*ADDRESS_WIDTH);
          break;
        case B:
          instr |= (uword_t(sym->second) << 1*ADDRESS_WIDTH);
          break;
        case J:
          instr |= (uword_t(sym->second) << 0*ADDRESS_WIDTH);
          break;
      }
    }
    
    references.erase(map_it++);
  }
  
  f.open(argv[2], fstream::out | fstream::binary);
  
  {
    uint32_t tmp;
    
    // write text section offset
    f.write((const char*)&text_offset, sizeof(address_t));
    
    // write number of exported symbols
    tmp = exported.size();
    f.write((const char*)&tmp, sizeof(uint32_t));
    
    // write exported symbols
    for (auto& exp : exported) {
      // string
      f.write(exp.c_str(), exp.size() + 1);
      
      // address
      tmp = symbols[exp];
      f.write((const char*)&tmp, sizeof(uint32_t));
    }
    
    // write number of symbols of pending references
    tmp = references.size();
    f.write((const char*)&tmp, sizeof(uint32_t));
    
    // write symbols of pending references
    for (auto& sym : references) {
      // string
      f.write(sym.first.c_str(), sym.first.size() + 1);
      
      // write number of references to current symbol
      tmp = sym.second.size();
      f.write((const char*)&tmp, sizeof(uint32_t));
      
      // write references to current symbol
      for (auto& ref : sym.second) {
        // address
        tmp = ref.first;
        f.write((const char*)&tmp, sizeof(uint32_t));
        
        // field
        tmp = ref.second;
        f.write((const char*)&tmp, sizeof(uint32_t));
      }
    }
    
    // write number of absolute addresses
    tmp = absolute.size();
    f.write((const char*)&tmp, sizeof(uint32_t));
    
    // write absolute addresses
    for (auto& addr : absolute) {
      // address
      tmp = addr.first;
      f.write((const char*)&tmp, sizeof(uint32_t));
      
      // field
      tmp = addr.second;
      f.write((const char*)&tmp, sizeof(uint32_t));
    }
    
    // write assembled code size
    f.write((const char*)&mem_size, sizeof(address_t));
    
    // write assembled code
    f.write((const char*)mem, sizeof(uword_t)*mem_size);
  }
  
  f.close();
  
  delete[] mem;
  
  return 0;
}
