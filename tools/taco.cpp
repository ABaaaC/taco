#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>

#include "taco/tensor_base.h"
#include "taco/expr.h"
#include "taco/operator.h"
#include "taco/parser/parser.h"
#include "taco/storage/storage.h"

#include "ir/ir.h"
#include "lower/lower_codegen.h"
#include "lower/iterators.h"
#include "lower/iteration_schedule.h"
#include "lower/merge_lattice.h"
#include "taco/util/error.h"
#include "taco/util/strings.h"
#include "taco/util/timers.h"
#include "taco/util/fill.h"
#include "taco/util/env.h"

using namespace std;
using namespace taco;

#define TOOL_BENCHMARK_REPEAT(CODE, NAME, REPEAT) {              \
    if (time) {                                                  \
      TACO_TIME_REPEAT(CODE,REPEAT,timevalue);                   \
      cout << NAME << " time (ms)" << endl << timevalue << endl; \
    }                                                            \
    else {                                                       \
      CODE;                                                      \
    }                                                            \
}

#define TOOL_BENCHMARK(CODE,NAME) {                              \
    if (time) {                                                  \
      taco::util::Timer timer;                                   \
      timer.start();                                             \
      CODE;                                                      \
      timer.stop();                                                    \
      cout << NAME << " " << timer.getResult() << " ms" << endl; \
    }                                                            \
    else {                                                       \
      CODE;                                                      \
    }                                                            \
}

static void printFlag(string flag, string text) {
  const size_t descriptionStart = 30;
  const size_t columnEnd        = 80;
  string flagString = "  -" + flag +
                      util::repeat(" ",descriptionStart-(flag.size()+3));
  cout << flagString;
  size_t column = flagString.size();
  vector<string> words = util::split(text, " ");
  for (auto& word : words) {
    if (column + word.size()+1 >= columnEnd) {
      cout << endl << util::repeat(" ", descriptionStart);
      column = descriptionStart;
    }
    column += word.size()+1;
    cout << word << " ";
  }
  cout << endl;
}

static void printUsageInfo() {
  cout << "Usage: taco <index expression> [options]" << endl;
  cout << endl;
  cout << "Examples:" << endl;
  cout << "  taco \"a(i) = b(i) + c(i)\"                            # Dense vector add" << endl;
  cout << "  taco \"a(i) = b(i) + c(i)\" -f=b:s -f=c:s -f=a:s       # Sparse vector add" << endl;
  cout << "  taco \"a(i) = B(i,j) * c(j)\" -f=B:ds                  # SpMV" << endl;
  cout << "  taco \"A(i,l) = B(i,j,k) * C(j,l) * D(k,l)\" -f=B:sss  # MTTKRP" << endl;
  cout << endl;
  cout << "Options:" << endl;
  printFlag("d=<dimension-size>",
            "Specify the size of tensor dimension by specifying the size of "
            "index variables that index them. All sizes defaults to 42. "
            "Examples: i:5 and j:100.");
  cout << endl;
  printFlag("f=<format>",
            "Specify the format of a tensor in the expression. Formats are "
            "specified per dimension using d (dense) and s (sparse). "
            "All formats default to dense. "
            "Examples: A:ds, b:d and D:sss.");
  cout << endl;
  printFlag("i=<file>",
            "Read a matrix from file in HB or MTX file format.");
  cout << endl;
  printFlag("o",
            "Write the result of evaluating the expression to tmpdir");
  cout << endl;
  printFlag("g=<fill>",
            "Generate random data for a given tensor. Vectors can be "
            "d (dense sequence), r (dense random), s (sparse) or h "
            "(hypersparse). Matrices can be d, s, h or "
            "l (slicing), f (FEM), b (Blocked).");
  cout << endl;
  printFlag("time=<repeat>",
            "Time compilation, assembly and <repeat> times computation. "
            "<repeat> is optional and defaults to 1.");
  cout << endl;
  printFlag("verify",
            "Verify results when comparing kernels.");
  cout << endl;
  printFlag("print-compute",
            "Print the compute kernel (default).");
  cout << endl;
  printFlag("print-assembly",
            "Print the assembly kernel.");
  cout << endl;
  printFlag("print-lattice=<var>",
            "Print merge lattice IR for the given index variable.");
  cout << endl;
  printFlag("nocolor", "Print without colors.");
  cout << endl;
  printFlag("write-source=<filename>",
            "Write the C source code of the kernel functions to a file.");
  cout << endl;
  printFlag("read-source=<filename>",
            "Read the C source code of the kernel functions from a file. "
            "The code must implement the given expression on the given "
            "formats. If tensor values are loaded or generated then the "
            "given expression and kernel functions are executed and compared. "
            "If the -time option is used then the given expression and "
            "kernels are timed.");
}

static int reportError(string errorMessage, int errorCode) {
  cerr << "Error: " << errorMessage << endl << endl;
  printUsageInfo();
  return errorCode;
}

void printCommandLine(ostream& os, int argc, char* argv[]) {
  os << "$ ";
  taco_iassert(argc > 0);
  os << argv[0];
  if (argc > 1) {
    os << " \"" << argv[1] << "\"";
  }
  for (int i = 2; i < argc; i++) {
    os << " " << argv[i];
  }
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    printUsageInfo();
    return 0;
  }

  bool printCompute  = false;
  bool printAssemble = false;
  bool printLattice  = false;
  bool printOutput   = false;
  bool writeKernels  = false;
  bool loaded        = false;
  bool verify        = false;
  bool time          = false;

  bool color         = true;
  bool readKernels   = false;

  int  repeat = 1;
  taco::util::TimeResults timevalue;

  string indexVarName = "";

  string exprStr;
  map<string,Format> formats;
  map<string,std::vector<int>> tensorsSize;
  map<string,taco::util::FillMethod> tensorsFill;
  map<string,string> tensorsFileNames;
  string writeKernelFilename;

  vector<string> kernelFilenames;

  for (int i = 1; i < argc; i++) {
    string arg = argv[i];
    vector<string> argparts = util::split(arg, "=");
    if (argparts.size() > 2) {
      return reportError("Too many '\"' signs in argument", 5);
    }
    string argName = argparts[0];
    string argValue = argparts[1];

    if ("-f" == argName) {
      vector<string> descriptor = util::split(argValue, ":");
      if (descriptor.size() < 2 || descriptor.size() > 3) {
        return reportError("Incorrect format descriptor", 3);
      }
      string tensorName     = descriptor[0];
      string formatString   = descriptor[1];
      Format::LevelTypes      levelTypes;
      Format::DimensionOrders dimensions;
      for (size_t i = 0; i < formatString.size(); i++) {
        switch (formatString[i]) {
          case 'd':
            levelTypes.push_back(LevelType::Dense);
            break;
          case 's':
            levelTypes.push_back(LevelType::Sparse);
            break;
          default:
            return reportError("Incorrect format descriptor", 3);
            break;
        }
        dimensions.push_back(i);
      }
      if (descriptor.size() > 2) {
        string dimOrderString = descriptor[2];
        dimensions.clear();
        for (size_t i = 0; i < dimOrderString.size(); i++) {
          dimensions.push_back(dimOrderString[i] - '0');
        }
      }
      formats.insert({tensorName, Format(levelTypes, dimensions)});
    }
    else if ("-d" == argName) {
      vector<string> descriptor = util::split(argValue, ":");
      string tensorName = descriptor[0];
      vector<string> dimensions = util::split(descriptor[1], ",");
      vector<int> tensorDimensions;
      for (size_t j=0; j<dimensions.size(); j++ ) {
        tensorDimensions.push_back(std::stoi(dimensions[j]));
      }
      tensorsSize.insert({tensorName, tensorDimensions});

    }
    else if ("-g" == argName) {
      vector<string> descriptor = util::split(argValue, ":");
      if (descriptor.size() < 2 || descriptor.size() > 3) {
        return reportError("Incorrect generating descriptor", 3);
      }
      string tensorName = descriptor[0];
      std::vector<taco::util::FillMethod> fillMethods;
      string fillString = descriptor[1];
      switch (fillString[0]) {
        case 'd': {
          tensorsFill.insert({tensorName, taco::util::FillMethod::Dense});
          break;
        }
        case 'u': {
          tensorsFill.insert({tensorName, taco::util::FillMethod::Uniform});
          break;
        }
        case 'r': {
          tensorsFill.insert({tensorName, taco::util::FillMethod::Random});
          break;
        }
        case 's': {
          tensorsFill.insert({tensorName, taco::util::FillMethod::Sparse});
          break;
        }
        case 'h': {
          tensorsFill.insert({tensorName, taco::util::FillMethod::HyperSpace});
          break;
        }
        case 'v': {
          tensorsFill.insert({tensorName, taco::util::FillMethod::SlicingV});
          break;
        }
        case 'l': {
          tensorsFill.insert({tensorName, taco::util::FillMethod::SlicingH});
          break;
        }
        case 'f': {
          tensorsFill.insert({tensorName, taco::util::FillMethod::FEM});
          break;
        }
        case 'b': {
          tensorsFill.insert({tensorName, taco::util::FillMethod::Blocked});
          break;
        }
        default: {
          return reportError("Incorrect generating descriptor", 3);
          break;
        }
      }
      loaded = true;
    }
    else if ("-i" == argName) {
      vector<string> descriptor = util::split(argValue, ":");
      if (descriptor.size() != 2) {
        return reportError("Incorrect read descriptor", 3);
      }
      string tensorName = descriptor[0];
      string fileName  = descriptor[1];
      tensorsFileNames.insert({tensorName,fileName});
      loaded = true;
    }
    else if ("-o" == argName) {
      printOutput = true;
    }
    else if ("-print-compute" == argName) {
      printCompute = true;
    }
    else if ("-print-assembly" == argName) {
      printAssemble = true;
    }
    else if ("-print-lattice" == argName) {
      indexVarName = argValue;
      printLattice = true;
    }
    else if ("-nocolor" == argName) {
      color = false;
    }
    else if ("-time" == argName) {
      time = true;
      if (argValue != "") {
        try {
          repeat=stoi(argValue);
        }
        catch (...) {
          return reportError("Incorrect time descriptor", 3);
        }
      }
    }
    else if ("-verify" == argName) {
      verify = true;
    }
    else if ("-write-source" == argName) {
      writeKernelFilename = argValue;
      writeKernels = true;
    }
    else if ("-read-source" == argName) {
      kernelFilenames.push_back(argValue);
      readKernels = true;
    }
    else {
      if (exprStr.size() != 0) {
        printUsageInfo();
        return 2;
      }
      exprStr = argv[i];
    }
  }

  // Print compute is the default if nothing else was asked for
  if (!printAssemble && !printLattice && !loaded &&
      !writeKernels && !readKernels) {
    printCompute = true;
  }

  // Load tensors
  map<string,TensorBase> loadedTensors;

  // Load tensors
  for (auto& tensorNames : tensorsFileNames) {
    string name     = tensorNames.first;
    string filename = tensorNames.second;

    TensorBase tensor;
    TOOL_BENCHMARK(tensor = readTensor(filename,name), name+" file read:");

    Format format = util::contains(formats, name)
        ? formats.at(name)
        : Format(LevelType::Dense, tensor.getOrder());
    tensor.setFormat(format);

    TOOL_BENCHMARK(tensor.pack(), name+" pack:     ");

    loadedTensors.insert({name, tensor});

    cout << tensor.getName()
         << " size: "
         << "(" << util::join(tensor.getDimensions(), " x ") << "), "
         << tensor.getStorage().getStorageCost() << " bytes" << endl;
  }

  if (exprStr == "") {
    return 0;
  }

  TensorBase tensor;
  parser::Parser parser(exprStr, formats, tensorsSize, loadedTensors, 42);
  try {
    parser.parse();
    tensor = parser.getResultTensor();
  } catch (parser::ParseError& e) {
    return reportError(e.getMessage(), 6);
  }

  if (printLattice && !parser.hasIndexVar(indexVarName)) {
    return reportError("Index variable is not in expression", 4);
  }

  // Generate tensors
  for (auto& fills : tensorsFill) {
    TensorBase tensor = parser.getTensor(fills.first);
    util::fillTensor(tensor,fills.second);

    loadedTensors.insert({fills.first, tensor});
    cout << tensor.getName()
         << " size: "
         << "(" << util::join(tensor.getDimensions(), " x ") << "), "
         << tensor.getStorage().getStorageCost() << " bytes" << endl;
  }

  // If all input tensors have been initialized then we should evaluate
  bool evaluate = true;
  for (auto& tensor : parser.getTensors()) {
    if (tensor.second == parser.getResultTensor()) {
      continue;
    }
    if (!util::contains(loadedTensors, tensor.second.getName())) {
      evaluate = false;
    }
  }

  if (evaluate) {
    if (time) cout << endl;
    TOOL_BENCHMARK(tensor.compile(),   "Compile: ");
    TOOL_BENCHMARK(tensor.assemble(),  "Assemble:");
    if (repeat == 1) {
      TOOL_BENCHMARK(tensor.compute(), "Compute: ");
    }
    else {
      TOOL_BENCHMARK_REPEAT(tensor.compute(),  "Compute",  repeat);
    }

    for (auto& kernelFilename : kernelFilenames) {
      TensorBase kernelTensor;

      std::ifstream filestream;
      filestream.open(kernelFilename, std::ifstream::in);
      string kernelSource((std::istreambuf_iterator<char>(filestream)),
                          std::istreambuf_iterator<char>());
      filestream.close();

      // TODO: Replace this redundant parsing with just a call to set the expr
      try {
        auto operands = parser.getTensors();
        operands.erase(parser.getResultTensor().getName());
        parser::Parser parser2(exprStr, formats, tensorsSize,
                               operands, 42);
        parser2.parse();
        kernelTensor = parser2.getResultTensor();
      } catch (parser::ParseError& e) {
        return reportError(e.getMessage(), 6);
      }
      kernelTensor.compileSource(kernelSource);

      if (time) {
        cout << endl;
        cout << kernelFilename << ":" << endl;
      }
      TOOL_BENCHMARK(kernelTensor.assemble(), "Assemble:");
      TOOL_BENCHMARK(kernelTensor.compute(),  "Compute: ");

      if (verify) {
        if (time) cout << endl;
        cout << "Verifying... ";
        bool eq = equals(kernelTensor, tensor);
        cout << "done" << endl;
        if (!eq) {
          string errorMessage =
              "Results computed with " + kernelFilename +
              " differ from those computed with the expression.";
          cerr << "Error: " << errorMessage << endl;
          return 7;
        }
      }
    }
  }
  else {
    TOOL_BENCHMARK(tensor.compile(), "Compile");
  }

  string gentext = "Generated by the Tensor Algebra Compiler (tensor-compiler.org)";
  if (printAssemble || printCompute) {
    std::string green = (color) ? "\033[38;5;70m" : "";
    std::string nc    = (color) ? "\033[0m"       : "";
    cout << green << "/* " << gentext << " */" << nc << endl;
  }

  bool hasPrinted = false;
  if (printAssemble) {
    tensor.printAssemblyIR(cout,color);
    hasPrinted = true;
    std::cout << std::endl;
  }

  if (printCompute) {
    if (hasPrinted) {
      cout << endl;
    }
    tensor.printComputeIR(cout,color);
    hasPrinted = true;
    std::cout << std::endl;
  }

  if (printLattice) {
    if (hasPrinted) {
      cout << endl << endl;
    }
    taco::Var indexVar = parser.getIndexVar(indexVarName);
    lower::IterationSchedule schedule = lower::IterationSchedule::make(tensor);
    map<TensorBase,ir::Expr> tensorVars;
    tie(std::ignore, std::ignore, tensorVars) = lower::getTensorVars(tensor);
    lower::Iterators iterators(schedule, tensorVars);
    auto lattice = lower::MergeLattice::make(tensor.getExpr(), indexVar,
                                             schedule, iterators);
    cout << lattice << endl;
    hasPrinted = true;
  }

  if (writeKernels) {
    std::ofstream filestream;
    filestream.open(writeKernelFilename, std::ofstream::out|std::ofstream::trunc);
    filestream << "/*" << endl << " " << gentext << endl;
    filestream << " ";
    printCommandLine(filestream, argc, argv);
    filestream << endl << "*/" << endl;
    filestream << tensor.getSource();
    filestream.close();
  }

  if (printOutput) {
    string tmpdir = util::getTmpdir();
    string outputFileName = tmpdir + "/" + tensor.getName() + ".mtx";
    tensor.writeMTX(outputFileName);
    TensorBase paramTensor;
    for ( const auto &fills : tensorsFill ) {
      paramTensor = parser.getTensor(fills.first);
      outputFileName = tmpdir + "/" + paramTensor.getName() + ".mtx";
      paramTensor.writeMTX(outputFileName);
    }
  }

  return 0;
}
