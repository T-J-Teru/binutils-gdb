struct gdb_split_args
{
  gdb_split_args (const std::string &args)
  {
    const char *input = args.c_str ();
    bool squote = false, dquote = false, bsquote = false;
    std::string arg;

    input = skip_spaces (input);

    do
      {
	while (*input != '\0')
	  {
	    /* TODO: Use 'ISSPACE'.  */
	    if (isspace (*input) && !squote && !dquote)
	      {
		break;
	      }
	    else
	      {
		if (bsquote)
		  {
		    arg += *input;
		    bsquote = false;
		  }
		else if (*input == '\\')
		  {
		    bsquote = true;

		    if (false && dquote && input[1] == '"')
		      {
			arg += input[1];
			++input;
		      }
		    else
		      {
			arg += *input;
		      }
		  }
		else if (squote)
		  {
		    if (*input == '\'')
		      {
			squote = false;
		      }
		    else
		      {
			arg += *input;
		      }
		  }
		else if (dquote)
		  {
		    if (*input == '"')
		      {
			dquote = false;
		      }
		    else if (*input == '\'')
		      {
			arg += '\\';
			arg += *input;
		      }
		    else
		      {
			arg += *input;
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
	  }

	m_argv.push_back (std::move (arg));
	arg.clear ();

	input = skip_spaces (input);
      }
    while (*input != '\0');
  }

private:

  std::vector<std::string> m_argv;

public:

  using iterator = decltype (m_argv)::iterator;

  iterator begin ()
  {
    return m_argv.begin ();
  }

  iterator end ()
  {
    return m_argv.end ();
  }
};
