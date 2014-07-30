/**
 * SUBLEQ Assembly: main.cpp
 *
 *  Created on: Apr 9, 2014
 *      Author:       Pimenta
 *      Collaborator: Alexandre Dantas
 *
 * This program reads a SUBLEQ Assembly file and outputs
 * a well-formed object file.
 * It's supposed to be fed to the Linker.
 */

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <sstream>
#include <iostream>

using namespace std;

typedef uint32_t uword_t;

#ifndef MEM_WORDS
#define MEM_WORDS 0x2000
#endif

/// Buffer that contains the entire contents
/// of the input file, with normalized line endings.
static stringstream f;

/// Global buffer that always contains the current
/// token being read by `readToken`
static string buf;

/// Set of all exported symbols.
/// They're used together with other object files
/// on the linker.
static set<string> exported;

/// All current symbols declared on this file.
/// Maps from a label to it's address in memory.
static map<string, uword_t> symbols;

/// Contains all symbols not found on this file.
///
/// It has labels supposed to be found on other
/// Assembly files.
/// They'll get resolved with the external symbols.
static map<string, set<uword_t>> references;

/// Relative addresses found on this file.
///
/// When you "call" a symbol on this file, it's address
/// is relative to the start of this file.
///
/// When the Linker joins several object files, it will
/// use the values on this set to relocate them all.
///
static set<uword_t> relatives;

/// Current size of the memory.
/// While parsing: points the current memory address
///                being read.
/// At the end:    has the entire size of the memory
///                read.
static uword_t mem_size = 0;

/// Raw memory.
/// It contains the assembled memory that will get
/// written at the end of the object file.
static uword_t* mem = new uword_t[MEM_WORDS];

static int currentLine = 1, lastTokenLine = 1, currentTokenLine = 1;

/// Reads a string from `is` to the end-of-line, saving it on `t`.
///
/// @note It doesn't save the end-of-line character on `t`.
///
/// This is a "safe" implementation of `std::getline`.
/// It is "safe" in the sense that it does work with all possible
/// line endings ("\r", "\n" and "\r\n").
///
/// Taken straight from StackOverflow. Source:
/// http://stackoverflow.com/a/6089413
///
std::istream& safeGetline(std::istream& is, std::string& t)
{
  t.clear();

  // The characters in the stream are read one-by-one using a
  // std::streambuf. That is faster than reading them one-by-one
  // using the std::istream.
  //
  // Code that uses streambuf this way must be guarded by a sentry
  // object.
  // The sentry object performs various tasks, such as thread
  // synchronization and updating the stream state.

  std::istream::sentry se(is, true);

  std::streambuf* sb = is.rdbuf();

  while (true) {
    int c = sb->sbumpc();

    switch (c) {

    case '\n':
      return is;

    case '\r':
      if (sb->sgetc() == '\n')
        sb->sbumpc();

      return is;

    case EOF:
      // Also handle the case when the last line has no line ending
      if (t.empty())
        is.setstate(std::ios::eofbit);

      return is;

    default:
      t += (char)c;
    }
  }
}

/// Reads a token (separated by whitespaces) from the file,
/// char by char, putting on `buf`
inline static void readToken() {
  buf = "";
  lastTokenLine = currentTokenLine;

  // Will read all chars until:
  //
  // - Finding a whitespace (or tab)
  // - Finding a comment
  // - Finding an end-of-line
  // - File somehow ends
  //
  while (true)
  {
    char c = f.get();

    if (!f.good())
      break;

    // comment found
    if (c == '/') {

      // token was read
      if (buf.size()) {
        currentTokenLine = currentLine;
        return;
      }

      // ignoring entire comment
      do {
        c = f.get();
      } while (c != '\n' && f.good());

      currentLine++;
      continue;
    }

    // line break found
    if (c == '\n') {

      // token was read
      if (buf.size()) {
        currentTokenLine = currentLine++;
        return;
      }

      currentLine++;
      continue;
    }

    // white space found
    if (c == ' ' || c == '\t') {

      // token was read
      if (buf.size()) {
        currentTokenLine = currentLine;
        return;
      }
      continue;
    }

    // this character was none of the above
    // simply appending it
    buf += c;
  }

  currentTokenLine = currentLine;
}

/// Returns text on `buf` as data.
///
/// Converts textual data to words.
/// For example, converts "0x1" to 1.
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

/// Parses a single field contained on `buf`.
///
/// So for `SUBLEQ A B C` we call this function three times,
/// once for each field A, B and C.
///
/// Returns the address being "called" by the field.
///
/// For example, if we have:
///
///     .data
///         one: 1
///     .text
///       loop:
///         one one loop
///
/// For `one` on `loop:` it returns 0, since it's the
/// address of the thing being "called" by the instruction.
///
inline static uword_t parseField() {
  uword_t field = 0;
  // hex notation means absolute address
  if (buf[0] == '0' && buf[1] == 'x') {
    sscanf(buf.c_str(), "%i", &field);
  }
  // symbol means an address that needs to be relocated later
  else {
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
    // symbol not found. leave a reference
    if (sym == symbols.end()) {
      references[buf].emplace(mem_size);
    }
    // symbol found. the field is the address of the symbol
    else {
      field = sym->second;
    }
  }
  return field;
}

int main(int argc, char* argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage mode: subleq-asm <assembly_file> <object_file>\n");
    return 0;
  }

  fstream input_file(argv[1]);

  if (!input_file) {
    cout << "Input file '" << argv[1] << "' doesn't exist" << endl;
    return EXIT_FAILURE;
  }

  // Read the whole file and store on a buffer.
  //
  // This way we won't need to worry about specific
  // line-endings, since it will look like it only
  // has '\n'.
  while (true) {
    string str;
    safeGetline(input_file, str);
    f << (str + '\n');

    if (!input_file)
      break;
  }
  input_file.close();

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
      readToken();
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

  // Now we go through each symbol (label), resolving references.
  //
  // If it's not on this file (`symbols` map), we assume it's
  // found on other file (`references` map).
  //
  for (auto map_it = references.begin(); map_it != references.end(); ++map_it) {

    // Which symbol was referenced
    string symbol_name = map_it->first;

    // All addresses where the symbol above was referenced
    set<uword_t> symbol_called = map_it->second;

    // Is this symbol declared on this file?
    // If not then leave it be at `references`
    auto sym = symbols.find(symbol_name);

    if (sym == symbols.end())
      continue;

    // If it's on this file, we replace it's address on every
    // memory location that called it
    uword_t symbol_address = sym->second;

    for (auto it = symbol_called.begin(); it != symbol_called.end(); ++it)
      mem[*it] += symbol_address;

    // And remove it from the external reference map
    references.erase(map_it);
  }

  // Now, outputting the binary stuff
  fstream output_file(argv[2], fstream::out | fstream::binary);

  if (!output_file) {
    cout << "Couldn't write to file '" << argv[2] << "'" << endl;
    return EXIT_FAILURE;
  }

  {
    uword_t tmp;

    // write number of exported symbols
    tmp = exported.size();
    output_file.write((const char*)&tmp, sizeof(uword_t));

    // write exported symbols
    for (auto& exp : exported) {
      // string
      output_file.write(exp.c_str(), exp.size() + 1);

      // address
      tmp = symbols[exp];
      output_file.write((const char*)&tmp, sizeof(uword_t));
    }

    // write number of symbols of pending references
    tmp = references.size();
    output_file.write((const char*)&tmp, sizeof(uword_t));

    // write symbols of pending references
    for (auto& sym : references) {
      // string
      output_file.write(sym.first.c_str(), sym.first.size() + 1);

      // write number of references to current symbol
      tmp = sym.second.size();
      output_file.write((const char*)&tmp, sizeof(uword_t));

      // write references to current symbol
      for (auto ref : sym.second) {
        tmp = ref;
        output_file.write((const char*)&tmp, sizeof(uword_t));
      }
    }

    // write number of relative addresses
    tmp = relatives.size();
    output_file.write((const char*)&tmp, sizeof(uword_t));

    // write relative addresses
    for (auto addr : relatives) {
      tmp = addr;
      output_file.write((const char*)&tmp, sizeof(uword_t));
    }

    // write assembled code size
    output_file.write((const char*)&mem_size, sizeof(uword_t));

    // write assembled code
    output_file.write((const char*)mem, sizeof(uword_t)*mem_size);
  }

  output_file.close();

  delete[] mem;

  return 0;
}
