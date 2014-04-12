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
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <string>
#include <utility>

using namespace std;

typedef int64_t  word_t;
typedef uint64_t uword_t;
typedef int32_t  address_t;

enum field_t {
  A, B, J
};

typedef map<string, list<pair<address_t, field_t>>>::iterator map_iterator;
typedef list<pair<address_t, field_t>>::iterator list_iterator;

#ifndef MEM_WORDS
#define MEM_WORDS 8192
#endif

const address_t ADDRESS_MASK  = MEM_WORDS - 1;
const uword_t   ADDRESS_WIDTH = log2(MEM_WORDS);
const uword_t   A_MASK        = ~(((uword_t)(MEM_WORDS - 1)) << 2*ADDRESS_WIDTH);
const uword_t   B_MASK        = ~(((uword_t)(MEM_WORDS - 1)) << 1*ADDRESS_WIDTH);
const uword_t   J_MASK        = ~(((uword_t)(MEM_WORDS - 1)) << 0*ADDRESS_WIDTH);

int main(int argc, char* argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage mode: subleq-assembler <subleq_assembly_file> <binary_output>\n");
    return 0;
  }
  
  fstream f;
  string buf;
  uword_t word;
  set<string> exported;
  map<string, address_t> symbols;
  map<string, list<pair<address_t, field_t>>> references;
  address_t ip = 0;
  uword_t mem[MEM_WORDS];
  
  f.open(argv[1]);
  
  while (f.get() != '\n'); // skip "{.export\n}"
  
  do {
    f >> buf;
    if (buf != ".text")
      exported.insert(buf);
  } while (buf != ".text");
  
  while (f.get() != '\n'); // skip ".text{\n}"
  
  do {
    f >> buf;
    if (buf[buf.size() - 1] == ':')
      symbols[buf.substr(0, buf.size() - 1)] = ip;
    else if (buf != ".data") {
      if (symbols.find(buf) == symbols.end())
        references[buf].push_back(pair<address_t, field_t>(ip, A));
      else
        word  = ((uword_t)((symbols[buf] - ip) & ADDRESS_MASK)) << 2*ADDRESS_WIDTH;
      f >> buf;
      if (symbols.find(buf) == symbols.end())
        references[buf].push_back(pair<address_t, field_t>(ip, B));
      else
        word |= ((uword_t)((symbols[buf] - ip) & ADDRESS_MASK)) << 1*ADDRESS_WIDTH;
      f >> buf;
      if (symbols.find(buf) == symbols.end())
        references[buf].push_back(pair<address_t, field_t>(ip, J));
      else
        word |= ((uword_t)((symbols[buf] - ip) & ADDRESS_MASK)) << 0*ADDRESS_WIDTH;
      mem[ip++] = word;
    }
  } while (buf != ".data");
  
  while (f.get() != '\n'); // skip ".data{\n}"
  
  do {
    f >> buf;
    if (!f.fail()) {
      symbols[buf] = ip;
      f >> mem[ip++];
    }
  } while (!f.eof());
  
  f.close();
  
  // solve references
  for (map_iterator map_it = references.begin(); map_it != references.end();) {
    // external symbols
    if (symbols.find(map_it->first) == symbols.end()) {
      ++map_it;
      continue;
    }
    
    // solve
    for (list_iterator list_it = map_it->second.begin(); list_it != map_it->second.end(); ++list_it) {
      switch (list_it->second) {
        case A:
          mem[list_it->first] &= A_MASK;
          mem[list_it->first] |= ((uword_t)((symbols[map_it->first] - list_it->first) & ADDRESS_MASK)) << 2*ADDRESS_WIDTH;
          break;
        case B:
          mem[list_it->first] &= B_MASK;
          mem[list_it->first] |= ((uword_t)((symbols[map_it->first] - list_it->first) & ADDRESS_MASK)) << 1*ADDRESS_WIDTH;
          break;
        case J:
          mem[list_it->first] &= J_MASK;
          mem[list_it->first] |= ((uword_t)((symbols[map_it->first] - list_it->first) & ADDRESS_MASK)) << 0*ADDRESS_WIDTH;
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
    
    // write assembled code size
    f.write((const char*)&ip, sizeof(address_t));
    
    // write assembled code
    f.write((const char*)mem, sizeof(uword_t)*ip);
  }
  
  f.close();
  
  return 0;
}
