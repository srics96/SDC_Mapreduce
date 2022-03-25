#ifndef TRACE_H
#define TRACE_H 1

#include <iostream>
#include <string>

using namespace std;

class Trace {
 public:
  // Function name log
  std::string func_name_;
  // Function arg string log
  std::string args_str_;
  Trace(std::string func, std::string args_msg = "")
      : func_name_(func), args_str_(args_msg) {
    cout << ">>> Entering " << this->func_name_ << "(" << this->args_str_
              << ")" << endl;
  }
  ~Trace() { cout << "<<< Leaving " << this->func_name_ << endl; }
};
#endif /* ifndef TRACE_H */