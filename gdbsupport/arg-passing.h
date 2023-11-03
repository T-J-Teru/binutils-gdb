/* ... */

#include <string>
#include <vector>

struct remote_arg_handler
{
  virtual const char *name () const = 0;

  virtual std::vector<std::string> split (std::string args) = 0;

  virtual std::string join (std::vector<std::string> args) = 0;

  std::string join (std::vector<char *> args);
};

struct remote_arg_handler_factory
{
  static remote_arg_handler *get ();
};
