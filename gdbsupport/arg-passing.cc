/* ... */

#include "common-defs.h"
#include <stdlib.h>
#include <vector>
#include <memory>
#include "gdbsupport/gdb_unique_ptr.h"
#include "gdbsupport/arg-passing.h"
#include "gdbsupport/buildargv.h"
#include "gdbsupport/common-inferior.h"
#include "../gdb/temp-args.c"

struct args_as_c_strings
{
  args_as_c_strings (std::vector<std::string> args)
  {
    for (const auto & a : args)
      m_data.push_back (xstrdup (a.c_str ()));
  }

  ~args_as_c_strings ()
  {
    free_vector_argv (m_data);
  }

  std::vector<char *> get () const
  {
    return m_data;
  }

private:
  std::vector<char *> m_data;
};

struct remote_arg_handler_0 : public remote_arg_handler
{
  const char *name () const override
  {
    return "strategy_0";
  }

  std::vector<std::string>
  split (std::string args) override
  {
    std::vector<std::string> remote_args;

    gdb_argv argv (args.c_str ());
    for (int i = 0; argv[i] != nullptr; i++)
      remote_args.push_back (std::string (argv[i]));

    return remote_args;
  }

  std::string join (std::vector<std::string> args) override
  {
    args_as_c_strings c_args (args);

    return construct_inferior_arguments (c_args.get (),
					 escape_shell_characters);
  }
};

struct remote_arg_handler_1 : public remote_arg_handler
{
  const char *name () const override
  {
    return "strategy_1";
  }

  std::vector<std::string>
  split (std::string args) override
  {
    std::vector<std::string> remote_args;

    gdb_argv argv (args.c_str ());
    for (int i = 0; argv[i] != nullptr; i++)
      remote_args.push_back (std::string (argv[i]));

    return remote_args;
  }

  std::string join (std::vector<std::string> args) override
  {
    args_as_c_strings c_args (args);

    return construct_inferior_arguments (c_args.get (),
					 escape_white_space);
  }
};

struct remote_arg_handler_2 : public remote_arg_handler
{
  const char *name () const override
  {
    return "strategy_2";
  }

  std::vector<std::string>
  split (std::string args) override
  {
    std::vector<std::string> remote_args;

    gdb_split_args argv (args);
    for (const auto &a : argv)
      remote_args.push_back (a);

    return remote_args;
  }

  std::string join (std::vector<std::string> args) override
  {
    args_as_c_strings c_args (args);

    return construct_inferior_arguments (c_args.get (),
					 escape_some_stuff);
  }
};

struct remote_arg_handler_3 : public remote_arg_handler
{
  const char *name () const override
  {
    return "strategy_3";
  }

  std::vector<std::string>
  split (std::string args) override
  {
    std::vector<std::string> remote_args;

    const char *input = args.c_str ();
    bool squote = false, dquote = false, bsquote = false;
    std::string arg;

    /* TODO: This is only Unix shell characters.  */
    static const char special[] = "\"!#$&*()\\|[]{}<>?'`~^; \t\n";

    input = skip_spaces (input);

    do
      {
	while (*input != '\0')
	  {
	    if (isspace (*input) && !squote && !dquote && !bsquote)
	      {
		break;
	      }
	    else if (isspace (*input) && false)
	      {
		if (*input == '\n')
		  {
		    arg += '\'';
		    arg += *input;
		    arg += '\'';
		  }
		else
		  {
		    arg += '\\';
		    arg += *input;
		  }
	      }
	    else if (*input == '\\' && !squote)
	      {
		arg += input[0];
		arg += input[1];
		++input;
	      }
	    else if (squote)
	      {
		if (*input == '\'')
		  {
		    squote = false;
		  }
		else
		  {
		    if (*input == '\n')
		      {
			arg += '\'';
			arg += *input;
			arg += '\'';
		      }
		    else if (strchr (special, *input) != NULL)
		      {
			/* This is a shell special character.  */
			arg += '\\';
			arg += *input;
		      }
		    else
		      {
			arg += *input;
		      }
		  }
	      }
	    else if (dquote)
	      {
		if (*input == '"')
		  {
		    dquote = false;
		  }
		else
		  {
		    if (*input == '\n')
		      {
			arg += '\'';
			arg += *input;
			arg += '\'';
		      }
		    else if (strchr (special, *input) != NULL)
		      {
			/* This is a shell special character.  */
			arg += '\\';
			arg += *input;
		      }
		    else
		      {
			arg += *input;
		      }
		  }
	      }
	    else
	      {
		if (*input == '\'')
		  {
		    squote = true;
		  }
		else if (*input == '"')
		  {
		    dquote = true;
		  }
		else
		  {
		    arg += *input;
		  }
	      }
	    ++input;
	  }

	remote_args.push_back (std::move (arg));
	arg.clear ();

	input = skip_spaces (input);
      }
    while (*input != '\0');

    return remote_args;
  }

  static std::string
  do_escape (const char *arg)
  {
    if (*arg == '\0')
      return "''";
    else
      return arg;
  }

  std::string join (std::vector<std::string> args) override
  {
    args_as_c_strings c_args (args);

    return construct_inferior_arguments (c_args.get (),
					 remote_arg_handler_3::do_escape);
  }
};

static std::vector<std::unique_ptr<remote_arg_handler>> arg_handlers;

remote_arg_handler *
remote_arg_handler_factory::get ()
{
  const char *stratergy = getenv ("APB_REMOTE_STRATEGY");
  int stratergy_idx = 3;
  if (stratergy != nullptr)
    stratergy_idx = atoi (stratergy);

  gdb_assert (stratergy_idx >= 0);

  if (arg_handlers.size() < (stratergy_idx + 1))
    arg_handlers.resize (stratergy_idx + 1);

  if (arg_handlers[stratergy_idx] != nullptr)
    return arg_handlers[stratergy_idx].get ();

  /* Need to create the handler.  */
  std::unique_ptr<remote_arg_handler> new_handler;
  switch (stratergy_idx)
    {
    case 0:
      new_handler = gdb::make_unique<remote_arg_handler_0> ();
      break;

    case 1:
      new_handler = gdb::make_unique<remote_arg_handler_1> ();
      break;

    case 2:
      new_handler = gdb::make_unique<remote_arg_handler_2> ();
      break;

    case 3:
      new_handler = gdb::make_unique<remote_arg_handler_3> ();
      break;

    default:
      gdb_assert_not_reached ("unknown handler strategy");
    }

  gdb_assert (new_handler != nullptr);
  arg_handlers[stratergy_idx] = std::move (new_handler);

  return arg_handlers[stratergy_idx].get ();
}

std::string
remote_arg_handler::join (std::vector<char *> args)
{
  std::vector<std::string> arg_strings;

  for (const auto &a : args)
    arg_strings.push_back (std::string (a));

  return join (arg_strings);
}
