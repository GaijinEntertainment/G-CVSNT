#include "StdAfx.h"
#include <ctime>
#include <iostream>
#include <memory>

#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/Test.h>
#include <cppunit/CompilerOutputter.h>
#include <cppunit/TestResultCollector.h>
#include <cppunit/TestResult.h>

#include "suite.h"


namespace CppUnitTest
{
   CppUnit::Test *suite();
};

class MyTextTestListener : public CppUnit::TestListener
{
   virtual void startTest(CppUnit::Test *t)
   {
      std::cout << t->getName() << std::endl;
   }
};

int main(int argc, char **argv)
{
   std::cout << "Running unit test..." << std::endl;
   time_t time_before_test = time(0);

   bool success = true;
   {
      CppUnit::TextUi::TestRunner runner;
      std::auto_ptr<MyTextTestListener> ttl(new MyTextTestListener());
      runner.eventManager().addListener(ttl.get());
      CppUnit::Test *pclTestSuite = CppUnitTest::suite();
      runner.addTest(pclTestSuite);
      CppUnit::CompilerOutputter *outputter = CppUnit::CompilerOutputter::defaultOutputter(&runner.result(), std::cerr);
      runner.setOutputter(outputter);

      int idx;
      for (idx = 1; idx < argc; idx++)
      {
         success = runner.run(argv[idx]) && success;
      }
      if (argc < 2)
      {
         success = runner.run();
      }
   }

   time_t time_after_test = time(0);
   std::cout << "Unit test finished in " << (time_after_test - time_before_test) << " seconds." << std::endl;

   return success ? 0 : 1;
}

