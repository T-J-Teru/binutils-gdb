/* Fortran 90 module support in GDB.


     */

#include "defs.h"
#include "symtab.h"
#include "psympriv.h"
#include "f-module.h"

#include "command.h"
#include "gdb_string.h"
#include "hashtab.h"
#include "source.h"
#include "value.h"
#include "cp-support.h"

#include <ctype.h>

/* XXX: The following macro and two functions are now private. */

/* A fast way to get from a psymtab to its symtab (after the first time).  */
#define PSYMTAB_TO_SYMTAB(pst)  \
    ((pst) -> symtab != NULL ? (pst) -> symtab : psymtab_to_symtab (pst))

extern struct symtab *psymtab_to_symtab (struct partial_symtab *pst);
char *psymtab_to_fullname (struct partial_symtab *ps, int *fail);

/* A linked-list of pointers to this module's symbols.  */

struct modtab_symbol
{
  const struct symbol *sym;
  struct modtab_symbol *next;
};

/* Record of a particular module.

   Created with a pointer to the associated partial_symtab. This is used
   to populate (if needed) the list of symbols for this module the first
   time information is requested by the user.  */

struct modtab_entry
{
  char *name;
  const struct partial_symtab *psymtab;
  struct modtab_symbol *sym_list;
};

/* Keep a hash-table of all modules.  */

static struct htab *modtab = NULL;

/* Keep a record of the "current" module.

   Used when adding newly discovered symbols into the encompassing
   module.  */

static struct modtab_entry *open_module = NULL;

/* Hash function for a module-name.

   This is a copy of the code from htab_hash_string(), except that it
   converts every char in the module-name to lower-case before computing
   the hash value.

   This is done to cope with Fortran's case-insensitivity ...  */

static hashval_t
htab_hash_modname (const void *p)
{
  const struct modtab_entry *modptr = (const struct modtab_entry *) p;
  const unsigned char *str = (const unsigned char *) modptr->name;
  hashval_t r = 0;
  unsigned char c;

  while ((c = tolower(*str++)) != 0)
    r = r * 67 + c - 113;

  return r;
}

/* Equality function for a module-name.

   Used by hash-table code to detect collisions of the hashing-function.  */

static int
modname_eq (const struct modtab_entry *lhs, const struct modtab_entry *rhs)
{
  /* Case-insensitive string compare ...  */

  return strcasecmp (lhs->name, rhs->name) == 0;
}

/* Initialise the module hash-table.  */

static void
modtab_init ()
{
  modtab = htab_create_alloc (256, htab_hash_modname,
                              (int (*) (const void *, const void *)) modname_eq,
                              NULL, xcalloc, xfree);
}

/* Open a new record of this Fortran module.

   Squirrels away a copy of the associated partial_symtab for later use
   when (if) the object-file's symbols have not been fully initialised.  */

void
f_module_announce (const char *name, const struct partial_symtab *psymtab)
{
  struct modtab_entry *candidate, **slot;

  candidate = (struct modtab_entry *) xmalloc (sizeof (struct modtab_entry));
  candidate->name = (char *) xmalloc (strlen (name) + 1);
  strcpy (candidate->name, name);
  candidate->psymtab = psymtab;
  candidate->sym_list = NULL;

  slot = (struct modtab_entry **) htab_find_slot (modtab, candidate, INSERT);

  if (*slot == NULL)
    {
      *slot = candidate;
    }
  else
    {
      printf_filtered ("f_module_announce: module '%s' already present\n",
                       name);

      xfree (candidate->name);
      xfree (candidate);
    }
}

/* Make this the "current" Fortran module.

   All Fortran symbols encountered will be marked as being part of this
   module until f_module_leave () is called.  */

void
f_module_enter (const char *name)
{
  struct modtab_entry *found_entry;

  if (open_module)
    {
      printf_filtered ("f_module_enter: attempt to nest a module\n");
      return;
    }

  found_entry = f_module_lookup (name);

  if (found_entry)
    {
      open_module = found_entry;
    }
}

/* Exit from this Fortran module.

   After this call, there will be no "current" Fortran module. Any
   further Fortran symbols encountered will not be associated with
   the (now) previous Fortran module.  */

void
f_module_leave ()
{
  if (!open_module)
    {
      printf_filtered ("f_module_leave(): not currently in a module\n");
    }

  open_module = NULL;
}
  
/* Associate this symbol with the current Fortran module.  */

void
f_module_sym_add (const struct symbol *sym)
{
  if (!open_module)
    {
      printf_filtered ("f_module_sym_add: not currently in a module\n");
      return;
    }

  /* Some compilers create a DW_TAG_subprogram entry with the same
     name as the encompassing module. Ignore.  */
  if (strcasecmp (open_module->name, sym->ginfo.name) == 0)
    {
      return;
    }

  {
    struct modtab_symbol *msym;

    msym = (struct modtab_symbol *) xmalloc (sizeof (struct modtab_symbol));
    msym->sym = sym;
    msym->next = open_module->sym_list;
    open_module->sym_list = msym;
  }
}

/* Find a given Fortran modtab entry.  */

struct modtab_entry *
f_module_lookup (const char *module_name)
{
  struct modtab_entry search_entry, *found_entry;

  /* const-correctness gone to pot.  */
  search_entry.name = (char *) module_name;
  found_entry = htab_find (modtab, &search_entry);

  return found_entry;
}

/* Find a given symbol in a Fortran module entry.  */

struct symbol *
f_module_lookup_symbol (const struct modtab_entry *mte,
                        const char *symbol_name)
{
  struct symtab *s = NULL;
  const struct modtab_symbol *sym_list;

  if (mte->psymtab)
    {
      /* const-correctness gone to pot.  */

      /* Force the fullname to be filled in.  */
      if (mte->psymtab->fullname == NULL)
        {
          psymtab_to_fullname ((struct partial_symtab *) mte->psymtab, NULL);
        }
      
      s = PSYMTAB_TO_SYMTAB ((struct partial_symtab *) mte->psymtab);
    }

  /* Careful! Need to grab the sym_list AFTER converting the psymtab
     into a symtab (above).  */
  sym_list = mte->sym_list;

  while (sym_list)
    {
      const struct symbol *sym = sym_list->sym;

      if (strcasecmp (symbol_name, SYMBOL_PRINT_NAME (sym)) == 0)
        {
          /* const-correctness gone to pot.  */
          return (struct symbol *) sym;
        }

      sym_list = sym_list->next;
    }

  return NULL;
}

/* Find a non-local Fortran symbol.  */

struct symbol *
f_lookup_symbol_nonlocal (const char *name,
                          const struct block *block,
                          const domain_enum domain)
{
  struct symbol *sym = NULL;
  char *p;
  
  /* Does name contain '::'?  */
  p = strchr(name, ':');

  /* Also check that any '::' is NOT at the beginning of the name.  */
  if (p && p[1] == ':' && p != name)
    {
      const int module_name_len = p - name;
      char *module_name;
      const struct modtab_entry *module;

      module_name = (char *) xmalloc (module_name_len + 1);
      memcpy (module_name, name, module_name_len);
      module_name[module_name_len] = '\0';

      module = f_module_lookup (module_name);

      if (module)
        {
          char *symbol_name;

          symbol_name = (p + 2);

          sym = f_module_lookup_symbol (module, symbol_name);
        }

      xfree (module_name);
    }

  if (sym)
    {
      return sym;
    }

  /* If we can't find it in our module list, let the default symbol-lookup
     have a go.  */
  return cp_lookup_symbol_nonlocal (name, block, domain);
}

/* Pretty-print a module's name.

   Called during the walk of the hash-table of all modules.

   If this is NOT the first time it has been called (during this
   particular "walk"), put a nice comma into the output.  */

static int
print_module_name (void **slot, void *arg)
{
  const struct modtab_entry **module = (const struct modtab_entry **) slot;
  int *first = (int *) arg;

  if (*first)
    {
      *first = 0;
    }
  else
    {
      printf_filtered (", ");
    }

  printf_filtered ("%s", (*module)->name);

  return 1;
}

/* User-command. Retrieve and print a list of all Fortran modules.  */

static void
modules_info (char *ignore, int from_tty)
{
  int first = 1;

  printf_filtered ("All defined modules:\n\n");

  htab_traverse_noresize (modtab, print_module_name, &first);

  printf_filtered ("\n");
}

/* Pretty-print a module's symbols.

   Passed in the module concerned and an indication as to the sort of
   information (either variables or functions) required.

   If the full symbol-information for the module's object file has not
   yet been read, will cause that to happen first.  */

static int
print_module_symbols (const struct modtab_entry **slot, void *arg)
{
  const struct modtab_entry *module = *((const struct modtab_entry **) slot);
  const enum search_domain kind = *((const enum search_domain *) arg);
  const struct modtab_symbol *sym_list;

  printf_filtered ("\nModule %s:\n", module->name);

  if (module->psymtab)
    {
      /* const-correctness gone to pot.  */

      /* Force the fullname to be filled in.  */
      if (module->psymtab->fullname == NULL)
        {
          psymtab_to_fullname ((struct partial_symtab *) module->psymtab, NULL);
        }
      
      PSYMTAB_TO_SYMTAB ((struct partial_symtab *) module->psymtab);
    }

  /* Careful! Need to grab the sym_list AFTER converting the psymtab
     into a symtab (above).  */
  sym_list = module->sym_list;

  while (sym_list)
    {
      const struct symbol *sym = sym_list->sym;

      if ((kind == VARIABLES_DOMAIN && SYMBOL_CLASS (sym) != LOC_TYPEDEF &&
           SYMBOL_CLASS (sym) != LOC_BLOCK) ||
          (kind == FUNCTIONS_DOMAIN && SYMBOL_CLASS (sym) == LOC_BLOCK))
        {
          type_print (SYMBOL_TYPE (sym),
                      (SYMBOL_CLASS (sym) == LOC_TYPEDEF
                       ? "" : SYMBOL_PRINT_NAME (sym)),
                      gdb_stdout, 0);

          printf_filtered (";%s;%d;\n",
                           (module->psymtab->fullname ?
                            module->psymtab->fullname :
                            ""),
                           sym->line);
        }

      sym_list = sym_list->next;
    }

  return 1;
}

/* Retrieve and print info for a named Fortran module.

   If no module named, retrieve and print for ALL modules.

   Needs to be told the sort of information required. Currently,
   this is either functions or variables.  */

static void
module_symbol_info (char *module_name, enum search_domain kind, int from_tty)
{
  static const char *classnames[] =
    {
      "variable",
      "function"
    };

  if (module_name)
    {
      const struct modtab_entry *found_entry;

      printf_filtered ("All defined module %ss for \"%s\":\n",
                       classnames[(int) (kind - VARIABLES_DOMAIN)],
                       module_name);

      found_entry = f_module_lookup (module_name);

      if (found_entry)
        {
          print_module_symbols (&found_entry, &kind);
        }
    }
  else
    {
      printf_filtered ("All defined module %ss:\n",
                       classnames[(int) (kind - VARIABLES_DOMAIN)]);

      htab_traverse_noresize (modtab, (int (*) (void **slot, void *arg)) print_module_symbols, &kind);
    }

  printf_filtered ("\n");
}

/* User-command. Retrieve and print a list of all functions for the
   named Fortran module.
   
   If no module named, do all modules.  */

static void
module_functions (char *module_name, int from_tty)
{
  module_symbol_info (module_name, FUNCTIONS_DOMAIN, from_tty);
}

/* User-command. Retrieve and print a list of all variables for the
   named Fortran module.
   
   If no module named, do all modules.  */

static void
module_variables (char *module_name, int from_tty)
{
  module_symbol_info (module_name, VARIABLES_DOMAIN, from_tty);
}

/* Initialise the Fortran module code.  */

extern void _initialize_f_module (void);

void
_initialize_f_module (void)
{
  modtab_init ();

  add_info ("modules", modules_info,
            _("All Fortran 90 modules."));
  add_info ("module_functions", module_functions,
            _("All global functions for the named Fortran 90 module."));
  add_info ("module_variables", module_variables,
            _("All global variables for the named Fortran 90 module."));
}
