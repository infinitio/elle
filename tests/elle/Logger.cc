//#define ELLE_TEST_NO_MEMFRY
#include <elle/test.hh>

#include <elle/finally.hh>
#include <elle/log.hh>
#include <elle/log/Logger.hh>
#include <elle/log/TextLogger.hh>
#include <elle/memory.hh>
#include <elle/os/environ.hh>
#include <elle/os/environ.hh>
#include <elle/os/environ.hh>

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <sstream>
#include <thread>

template<bool b>
void
_message_test(bool env)
{
  using Level = elle::log::Logger::Level;

  std::stringstream ss;
  elle::log::TextLogger* logger;
  if (env)
  {
    elle::os::setenv("ELLE_LOG_LEVEL", "DUMP");
    elle::os::setenv("ELLE_LOG_DISPLAY_TYPE", "1");
    logger = new elle::log::TextLogger(ss);
  }
  else
  {
    logger = new elle::log::TextLogger(ss, "DUMP", true);
  }
  elle::log::logger(std::unique_ptr<elle::log::Logger>(logger));
  BOOST_CHECK_EQUAL(logger->component_level("Test"), Level::dump);

  {
    ELLE_LOG_COMPONENT("Test");
    ELLE_LOG_SCOPE("Test Message");
    BOOST_CHECK_EQUAL(ss.str(), "[1m[Test] Test Message\n[0m");

    ss.str("");
    ELLE_LOG("Another Test Message");
    BOOST_CHECK_EQUAL(ss.str(), "[1m[Test]   Another Test Message\n[0m");

    {
      ELLE_LOG_COMPONENT("Another");

      ss.str("");
      ELLE_LOG("Test");
      BOOST_CHECK_EQUAL(ss.str(), "[1m[Another]   Test\n[0m");

      ss.str("");
      ELLE_TRACE("Test2");
      BOOST_CHECK_EQUAL(ss.str(), "[Another]   Test2\n");

      ss.str("");
      ELLE_DEBUG("Test3");
      BOOST_CHECK_EQUAL(ss.str(), "[Another]   Test3\n");

      ss.str("");
      ELLE_DUMP("Test4");
      BOOST_CHECK_EQUAL(ss.str(), "[Another]   Test4\n");

      ss.str("");
      ELLE_DUMP("Test5")
      {
        BOOST_CHECK_EQUAL(ss.str(), "[Another]   Test5\n");
        ss.str("");
        ELLE_DUMP("Test5.1")
        {
          BOOST_CHECK_EQUAL(ss.str(), "[Another]     Test5.1\n");
          ss.str("");
          ELLE_DUMP("Test5.1.1");
          BOOST_CHECK_EQUAL(ss.str(), "[Another]       Test5.1.1\n");
        }
      }
    }

    ss.str("");
    ELLE_WARN("Test5");
    BOOST_CHECK_EQUAL(ss.str(), "[33;01;33m[ Test  ] [warning]   Test5\n[0m");

    ss.str("");
    ELLE_ERR("Test6");
    BOOST_CHECK_EQUAL(ss.str(), "[33;01;31m[ Test  ] [error]   Test6\n[0m");
  }
}

static
void
message_test()
{
  _message_test<false>(false);
  _message_test<true>(true);
}

static
void
clear_env()
{
  elle::os::unsetenv("ELLE_LOG_LEVEL");
  elle::os::unsetenv("ELLE_LOG_TIME");
  elle::os::unsetenv("ELLE_LOG_TIME_UNIVERSAL");
  elle::os::unsetenv("ELLE_LOG_PID");
}

static
void
_environment_format_test(bool env)
{
  ELLE_LOG_COMPONENT("Test");

  using Level = elle::log::Logger::Level;

  std::stringstream ss, res;
  elle::log::TextLogger* logger;

  ss.str("");
  res.str("");
  clear_env();
  if (env)
  {
    // FIXME: Checking time printing is non-deterministic.
    // elle::os::setenv("ELLE_LOG_TIME", "1", 0);
    // BOOST_CHECK_EQUAL(elle::os::getenv("ELLE_LOG_TIME"), "1");
    BOOST_CHECK_EQUAL(elle::os::getenv("ELLE_LOG_TIME_UNIVERSAL", ""), "");
    BOOST_CHECK_EQUAL(elle::os::getenv("ELLE_LOG_PID", ""), "");
    logger = new elle::log::TextLogger(ss);
  }
  else
  {
    logger = new elle::log::TextLogger(
      ss,
      "",     // log_level
      false,  // display_type
      false,  // enable_pid
      false,  // enable_tid
      // FIXME: Checking time printing is non-deterministic.
      // true,   // enable_time
      false,
      false   // universal_time
    );
  }
  elle::log::logger(std::unique_ptr<elle::log::Logger>(logger));
  BOOST_CHECK_EQUAL(logger->component_level("Test"),
                    Level::log);
  ELLE_LOG("Test");
  // FIXME: Checking time printing is non-deterministic.
  // auto time = boost::posix_time::second_clock::local_time();
  // res << "[1m" << time << ": [Test] Test\n[0m";
  res << "[1m[Test] Test\n[0m";
  BOOST_CHECK_EQUAL(ss.str(), res.str());

  ss.str("");
  res.str("");
  clear_env();
  if (env)
  {
    // FIXME: Checking time printing is non-deterministic.
    // elle::os::setenv("ELLE_LOG_TIME", "1", 0);
    // elle::os::setenv("ELLE_LOG_TIME_UNIVERSAL", "1", 0);
    // BOOST_CHECK_EQUAL(elle::os::getenv("ELLE_LOG_TIME"), "1");
    // BOOST_CHECK_EQUAL(elle::os::getenv("ELLE_LOG_TIME_UNIVERSAL"), "1");
    BOOST_CHECK_EQUAL(elle::os::getenv("ELLE_LOG_PID", ""), "");
    logger = new elle::log::TextLogger(ss);
  }
  else
  {
    logger = new elle::log::TextLogger(
      ss,
      "",     // log_level
      false,  // display_type
      false,  // enable_pid
      false,  // enable_tid
      // FIXME: Checking time printing is non-deterministic.
      // true,   // enable_time
      false,
      true    // universal_time
    );
  }
  elle::log::logger(std::unique_ptr<elle::log::Logger>(logger));
  BOOST_CHECK_EQUAL(logger->component_level("Test"),
                    Level::log);

  ELLE_LOG("Test 2");
  // FIXME: Checking time printing is non-deterministic.
  // res << "[1m"
  //     << Clock::now() << ": "
  //     << "[Test] Test 2\n[0m";
  res << "[1m[Test] Test 2\n[0m";
  BOOST_CHECK_EQUAL(ss.str(), res.str());

  ss.str("");
  res.str("");
  clear_env();
  if (env)
  {
    elle::os::setenv("ELLE_LOG_PID", "1", 0);
    BOOST_CHECK_EQUAL(elle::os::getenv("ELLE_LOG_TIME", ""), "");
    BOOST_CHECK_EQUAL(elle::os::getenv("ELLE_LOG_TIME_UNIVERSAL", ""), "");
    BOOST_CHECK_EQUAL(elle::os::getenv("ELLE_LOG_PID"), "1");
    logger = new elle::log::TextLogger(ss);
  }
  else
  {
    logger = new elle::log::TextLogger(
      ss,
      "",     // log_level
      false,  // display_type
      true,   // enable_pid
      false,  // enable_tid
      false,  // enable_time
      false   // universal_time
    );
  }
  elle::log::logger(std::unique_ptr<elle::log::Logger>(logger));
  BOOST_CHECK_EQUAL(logger->component_level("Test"),
                    Level::log);
  ELLE_LOG("Test 3");
  res << "[1m[Test] [" << boost::lexical_cast<std::string>(getpid()) << "] "
      << "Test 3\n[0m";
  BOOST_CHECK_EQUAL(ss.str(), res.str());

  ss.str("");
  res.str("");
  clear_env();
  if (env)
  {
    // FIXME: Checking time printing is non-deterministic.
    // elle::os::setenv("ELLE_LOG_TIME", "1", 0);
    // elle::os::setenv("ELLE_LOG_TIME_UNIVERSAL", "1", 0);
    // BOOST_CHECK_EQUAL(elle::os::getenv("ELLE_LOG_TIME"), "1");
    // BOOST_CHECK_EQUAL(elle::os::getenv("ELLE_LOG_TIME_UNIVERSAL"), "1");
    elle::os::setenv("ELLE_LOG_PID", "1", 0);
    BOOST_CHECK_EQUAL(elle::os::getenv("ELLE_LOG_PID"), "1");
    logger = new elle::log::TextLogger(ss);
  }
  else
  {
    logger = new elle::log::TextLogger(
      ss,
      "",     // log_level
      false,  // display_type
      true,   // enable_pid
      false,  // enable_tid
      // FIXME: Checking time printing is non-deterministic.
      // true,   // enable_time
      // true    // universal_time
      false,
      false
    );
  }
  elle::log::logger(std::unique_ptr<elle::log::Logger>(logger));
  BOOST_CHECK_EQUAL(logger->component_level("Test"),
                    Level::log);
  ELLE_LOG("Test 4");
  // FIXME: Checking time printing is non-deterministic.
  // res << "[1m"
  //     << Clock::now() << ": "
  //     << "[Test] [" << boost::lexical_cast<std::string>(getpid()) << "] "
  //     << "Test 4\n[0m";
  res << "[1m[Test] [" << boost::lexical_cast<std::string>(getpid()) << "] "
      << "Test 4\n[0m";
  BOOST_CHECK_EQUAL(ss.str(), res.str());

  ss.str("");
  res.str("");
  clear_env();
  if (env)
  {
    // FIXME: Checking time printing is non-deterministic.
    // elle::os::setenv("ELLE_LOG_TIME", "1", 0);
    // elle::os::setenv("ELLE_LOG_PID", "1", 0);
    // BOOST_CHECK_EQUAL(elle::os::getenv("ELLE_LOG_TIME"), "1");
    // BOOST_CHECK_EQUAL(elle::os::getenv("ELLE_LOG_TIME_UNIVERSAL", ""), "");
    BOOST_CHECK_EQUAL(elle::os::getenv("ELLE_LOG_PID"), "1");
    logger = new elle::log::TextLogger(ss);
  }
  else
  {
    logger = new elle::log::TextLogger(
      ss,
      "",     // log_level
      false,  // display_type
      true,   // enable_pid
      false,  // enable_tid
      // FIXME: Checking time printing is non-deterministic.
      // true,   // enable_time
      false,
      false   // universal_time
    );
  }
  elle::log::logger(std::unique_ptr<elle::log::Logger>(logger));
  BOOST_CHECK_EQUAL(logger->component_level("Test"), Level::log);
  ELLE_WARN("Test 5");
  // FIXME: Checking time printing is non-deterministic.
  // res << "[33;01;33m"
  //     << boost::posix_time::second_clock::local_time() << ": "
  //     << "[Test] [" << boost::lexical_cast<std::string>(getpid()) << "] "
  //     << "[warning] Test 5\n[0m";
  res << "[33;01;33m[Test] ["
      << boost::lexical_cast<std::string>(getpid()) << "] "
      << "[warning] Test 5\n[0m";
  BOOST_CHECK_EQUAL(ss.str(), res.str());

  ss.str("");
  res.str("");
  clear_env();
  if (env)
  {
    elle::os::setenv("ELLE_LOG_DISPLAY_TYPE", "1");
    BOOST_CHECK_EQUAL(elle::os::getenv("ELLE_LOG_DISPLAY_TYPE"), "1");
    logger = new elle::log::TextLogger(ss);
  }
  else
  {
    logger = new elle::log::TextLogger(
      ss,
      "",     // log_level
      true,   // display_type
      false,  // enable_pid
      false,  // enable_tid
      false,  // enable_time
      false   // universal_time
    );
  }
  elle::log::logger(std::unique_ptr<elle::log::Logger>(logger));
  BOOST_CHECK_EQUAL(logger->component_level("Test"), Level::log);
  ELLE_WARN("Test 5");
  res << "[33;01;33m"
      << "[Test] [warning] Test 5\n[0m";
  BOOST_CHECK_EQUAL(ss.str(), res.str());
  ELLE_ERR("Test 6")
  res << "[33;01;33m"
      << "[Test] [error] Test 6\n[0m";
  elle::os::setenv("ELLE_LOG_DISPLAY_TYPE", "0");

  elle::log::logger(std::unique_ptr<elle::log::Logger>(nullptr));
}

static
void
environment_format_test()
{
  _environment_format_test(false);
  // _environment_format_test(true);
}

static
void
parallel_write()
{
  std::stringstream output;
  elle::log::TextLogger logger(output);
  logger.component_level("in");
  logger.component_level("out");

  auto action = [](int& counter)
    {
      using namespace boost::posix_time;
      ptime deadline = microsec_clock::local_time() + seconds(10);
      while (microsec_clock::local_time() < deadline && counter < 64)
      {
        ELLE_LOG_COMPONENT("out");
        ELLE_LOG("out")
        {
          ELLE_LOG_COMPONENT("in");
          ELLE_ERR("in");
        }
        ++counter;
      }
    };

  int c1 = 0;;
  int c2 = 0;
  {
    std::thread t1([&](){ action(c1); });
    std::thread t2([&](){ action(c2); });

    t1.join();
    t2.join();
  }

  BOOST_CHECK_GE(c1, 64);
  BOOST_CHECK_GE(c2, 64);
}

static
void
multiline()
{
  std::stringstream output;
  elle::os::setenv("ELLE_LOG_LEVEL", "DUMP");
  elle::log::logger(std::make_unique<elle::log::TextLogger>(output));
  ELLE_LOG_COMPONENT("multiline");
  ELLE_TRACE("This message\nis\nsplitted\n\ninto\r\n5 lines\n\n\r\n\r\r");
  auto expected =
    "[multiline] This message\n"
    "            is\n"
    "            splitted\n"
    "            into\n"
    "            5 lines\n";
  BOOST_CHECK_EQUAL(output.str(), expected);
}

/// Check that we components are displayed with just the needed width.
static
void
component_width()
{
  auto generate_log = []
  {
    std::stringstream output;
    elle::log::logger(std::make_unique<elle::log::TextLogger>(output));
    ELLE_LOG_COMPONENT("foo");
    ELLE_TRACE("foo.1")
    {
      ELLE_LOG_COMPONENT("bar");
      ELLE_TRACE("bar.1")
      {
        ELLE_LOG_COMPONENT("quuuuux");
        ELLE_DUMP("quuuuux.1");
      }
      ELLE_TRACE("bar.2");
    }
    ELLE_TRACE("foo.2");
    return output.str();
  };

  elle::os::setenv("ELLE_LOG_LEVEL", "DUMP");
  BOOST_CHECK_EQUAL(generate_log(),
                    "[foo] foo.1\n"
                    "[bar]   bar.1\n"
                    "[quuuuux]     quuuuux.1\n"
                    "[  bar  ]   bar.2\n"
                    "[  foo  ] foo.2\n");

  elle::os::setenv("ELLE_LOG_LEVEL", "TRACE");
  BOOST_CHECK_EQUAL(generate_log(),
                    "[foo] foo.1\n"
                    "[bar]   bar.1\n"
                    "[bar]   bar.2\n"
                    "[foo] foo.2\n");
}

/// Check the use of context filters: print logs only when nested in
/// some component.
static
void
nested()
{
  auto generate_log = []
  {
    std::stringstream output;
    elle::log::logger(std::make_unique<elle::log::TextLogger>(output));
    ELLE_LOG_COMPONENT("foo");
    ELLE_TRACE("foo.1")
    {
      ELLE_LOG_COMPONENT("bar");
      ELLE_TRACE("bar.1")
      {
        ELLE_LOG_COMPONENT("baz");
        ELLE_TRACE("baz.1");
        ELLE_TRACE("baz.2");
      }
      ELLE_TRACE("bar.2");
    }
    ELLE_TRACE("foo.2")
    {
      ELLE_LOG_COMPONENT("baz");
      ELLE_TRACE("baz.3");
      ELLE_TRACE("baz.4");
    }
    ELLE_TRACE("foo.3");
    return output.str();
  };

  elle::os::setenv("ELLE_LOG_LEVEL", "TRACE");
  BOOST_CHECK_EQUAL(generate_log(),
                    "[foo] foo.1\n"
                    "[bar]   bar.1\n"
                    "[baz]     baz.1\n"
                    "[baz]     baz.2\n"
                    "[bar]   bar.2\n"
                    "[foo] foo.2\n"
                    "[baz]   baz.3\n"
                    "[baz]   baz.4\n"
                    "[foo] foo.3\n");

  elle::os::setenv("ELLE_LOG_LEVEL", "baz:TRACE");
  BOOST_CHECK_EQUAL(generate_log(),
                    "[baz]     baz.1\n"
                    "[baz]     baz.2\n"
                    "[baz]   baz.3\n"
                    "[baz]   baz.4\n");

  elle::os::setenv("ELLE_LOG_LEVEL", "bar baz:TRACE");
  BOOST_CHECK_EQUAL(generate_log(),
                    "[baz]     baz.1\n"
                    "[baz]     baz.2\n");
}

static
void
trim()
{
  std::stringstream output;
  elle::os::setenv("ELLE_LOG_LEVEL", "DUMP");
  elle::log::logger(std::make_unique<elle::log::TextLogger>(output));
  ELLE_LOG_COMPONENT("trim");
  ELLE_TRACE("   \n\t\t\tThis message is trimmed !    \n\n\r\n\r\r\t ");
  BOOST_CHECK_EQUAL(output.str(), "[trim] This message is trimmed !\n");
}

static
void
error()
{
  ELLE_LOG_COMPONENT("error");
  {
    std::stringstream output;
    elle::log::logger(std::make_unique<elle::log::TextLogger>(output));
    {
      // We are passing through already executed code with a different logger
      ELLE_LOG_COMPONENT("elle.printf");
      ELLE_LOG("force component creation");
    }
    ELLE_LOG("invalid log", 42);
    ELLE_LOG("invalid log %s");
    BOOST_CHECK_GT(output.str().size(), 0);
  }
  {
    std::stringstream output;
    elle::log::detail::debug_formats(true);
    elle::log::logger(std::make_unique<elle::log::TextLogger>(output));
    {
      // We are passing through already executed code with a different logger
      ELLE_LOG_COMPONENT("elle.printf");
      ELLE_LOG("force component creation");
    }
    BOOST_CHECK_THROW(ELLE_LOG("invalid log", 42), elle::Exception);
    BOOST_CHECK_THROW(ELLE_LOG("invalid log %s"), elle::Exception);
    BOOST_CHECK(!output.str().empty());
    elle::log::detail::debug_formats(false);
  }
}

ELLE_TEST_SUITE()
{
  elle::log::detail::debug_formats(false);
  auto& suite = boost::unit_test::framework::master_test_suite();

  elle::os::setenv("ELLE_LOG_COLOR", "1", 0);
  boost::unit_test::test_suite* logger = BOOST_TEST_SUITE("logger");
  suite.add(logger);
  logger->add(BOOST_TEST_CASE(message_test));
  logger->add(BOOST_TEST_CASE(environment_format_test));

#ifndef INFINIT_ANDROID
  boost::unit_test::test_suite* concurrency = BOOST_TEST_SUITE("concurrency");
  suite.add(concurrency);
  concurrency->add(BOOST_TEST_CASE(std::bind(parallel_write)));

  boost::unit_test::test_suite* format = BOOST_TEST_SUITE("format");
  suite.add(format);
  format->add(BOOST_TEST_CASE(error));
  format->add(BOOST_TEST_CASE(multiline));
  format->add(BOOST_TEST_CASE(trim));
  format->add(BOOST_TEST_CASE(component_width));
  format->add(BOOST_TEST_CASE(nested));
#endif
}
