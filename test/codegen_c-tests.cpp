#include <regex>

#include "test.h"
#include "taco/tensor.h"
#include "taco/format.h"
#include "ir/ir.h"
#include "backends/module.h"

using namespace ::testing;
using namespace taco::ir;
using taco::typeOf;

struct BackendCTests : public Test {

};

TEST_F(BackendCTests, BuildModule) {
  auto add = Function::make("foobar", {Var::make("x", typeOf<int>())},
                            {}, Block::make({}));
  stringstream foo;

  Module mod;
  mod.addFunction(add);
  mod.compile();
  
  typedef int (*fnptr_t)(void**);
  int ten = 10;
  void* pack[] = {(void*)&ten};
  //fnptr_t func = (fnptr_t)mod.get_func("foobar");
  static_assert(sizeof(void*) == sizeof(fnptr_t),
  "Unable to cast dlsym() returned void pointer to function pointer");
  void* v_func_ptr = mod.getFunc("foobar");
  fnptr_t func;
  *reinterpret_cast<void**>(&func) = v_func_ptr;

  
  EXPECT_EQ(0, func(pack));
}

TEST_F(BackendCTests, BuildModuleWithStore) {
  auto var = Var::make("x", typeOf<int>());
  auto fn = Function::make("foobar", {Var::make("y", typeOf<double>())},
    {var},
    Block::make({Store::make(var, Literal::make(0), Literal::make(101))}));

  Module mod;
  mod.addFunction(fn);
  mod.compile();
  
  typedef int (*fnptr_t)(void**);
  
//  fnptr_t func = (fnptr_t)mod.get_func("foobar");
  static_assert(sizeof(void*) == sizeof(fnptr_t),
  "Unable to cast dlsym() returned void pointer to function pointer");
  void* v_func_ptr = mod.getFunc("foobar");
  fnptr_t func;
  *reinterpret_cast<void**>(&func) = v_func_ptr;
  
  int x = 22;
  double y = 1.8;
  // calling convention is output, inputs
  void* pack[] = {(void*)&x, (void*)&y};
  
  EXPECT_EQ(0, func(pack));
  EXPECT_EQ(101, x);
}

TEST_F(BackendCTests, CallModuleWithStore) {
  auto var = Var::make("x", typeOf<int>());
  auto fn = Function::make("foobar", {Var::make("y", typeOf<double>())},
    {var},
    Block::make({Store::make(var, Literal::make(0), Literal::make(99))}));
  
  auto var2 = Var::make("y", typeOf<double>());
  auto fn2 = Function::make("booper", {Var::make("x", typeOf<int>())},
    {var2},
    Block::make({Store::make(var2, Literal::make(0), Literal::make(-20.0))}));
  
  Module mod;
  mod.addFunction(fn);
  mod.addFunction(fn2);
  mod.compile();

  int x = 11;
  double y = 1.8;
  EXPECT_EQ(0, mod.callFuncPacked("foobar", {(void*)(&x), (void*)(&y)}));
  EXPECT_EQ(99, x);
  
  EXPECT_EQ(0, mod.callFuncPacked("booper", {(void*)&y, (void*)&x}));
  EXPECT_EQ(-20.0, y);
}

TEST_F(BackendCTests, FullVecAdd) {
  // implements:
  // for i = 0 to len
  //  a[i] = b[i] + c[i]
  auto a = Var::make("a", typeOf<float>());
  auto b = Var::make("b", typeOf<float>());
  auto c = Var::make("c", typeOf<float>());
  auto veclen = Var::make("len", typeOf<int>(), false);
  auto veclen_val = Load::make(veclen);
  auto i = Var::make("i", typeOf<int>(), false);

  auto fn = Function::make("vecadd",
    {veclen, b, c}, // inputs
    {a},    // outputs
    // body
    Block::make({
      For::make(i, Literal::make(0), veclen, Literal::make(1),
        Block::make({Store::make(a, i,
                                 Add::make(Load::make(b, i), Load::make(c, i)))
                    }))
      }));
  
  Module mod;
  mod.addFunction(fn);
  mod.compile();
  
  float vec_a[10] = {0};
  float vec_b[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  float vec_c[10] = {2, 4, 6, 8, 10, 12, 14, 16, 18, 20};
  int ten = 10;
  
  // calling convention is output, inputs
  void* pack[] = {(void*)(vec_a), (void*)(&ten),
                  (void*)(vec_b), (void*)(vec_c)};
  
  mod.callFuncPacked("vecadd", pack);
  
  for (int j=0; j<10; j++)
    EXPECT_EQ(vec_b[j] + vec_c[j], vec_a[j]);
}





