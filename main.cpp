/*
 * main.cpp
 *
 *  Created on: Apr 9, 2014
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

const uword_t ADDRESS_WIDTH = uword_t(log2(double(MEM_WORDS)));

fstream f;
string buf;
set<string> exported;
map<string, address_t> symbols;
map<string, list<pair<address_t, field_t>>> references;
list<pair<address_t, field_t>> absolute;
address_t mem_size = 0;
uword_t* mem = new uword_t[MEM_WORDS];

inline void readField(uword_t& instr, field_t field) {
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

int main(int argc, char* argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage mode: subleq-asm <assembly_file> <object_file>\n");
    return 0;
  }
  
  f.open(argv[1]);
  
  f >> buf; // reading ".export"
  
  for (f >> buf; buf != ".data"; f >> buf) { // reading export section
    exported.insert(buf);
  }
  
  for (f >> buf; buf != ".text"; f >> buf) { // reading data section
    symbols[buf.substr(0, buf.size() - 1)] = mem_size;
    f >> mem[mem_size++];
  }
  
  for (f >> buf; !f.eof(); f >> buf) { // reading text section
    if (buf[buf.size() - 1] == ':') { // label found
      symbols[buf.substr(0, buf.size() - 1)] = mem_size;
      
      if (buf == "start:") // exporting start label
        exported.insert(string("start"));
      
      continue;
    }
    
    uword_t instr = 0;
    readField(instr, A);
    f >> buf;
    readField(instr, B);
    f >> buf;
    readField(instr, J);
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
