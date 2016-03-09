/* Fortran 90 module support in GDB. */



#if !defined (F_MODULE_H)
#define F_MODULE_H 1

struct block;
struct modtab_entry;
struct partial_symtab;
struct symbol;
struct symtab;

/* Open a new record of this Fortran module.

   Squirrels away a copy of the associated partial_symtab for later use
   when (if) the object-file's symbols have not been fully initialised.  */

extern void f_module_announce (const char *name,
                               const struct partial_symtab *psymtab);

/* Make this the "current" Fortran module.

   All Fortran symbols encountered will be marked as being part of this
   module until f_module_leave () is called.  */

extern void f_module_enter (const char *name);

/* Exit from this Fortran module.

   After this call, there will be no "current" Fortran module. Any
   further Fortran symbols encountered will not be associated with
   the (now) previous Fortran module.  */

extern void f_module_leave (void);

/* Associate this symbol with the current Fortran module.  */

extern void f_module_sym_add (const struct symbol *sym); 

/* Find a given Fortran modtab entry.  */

extern struct modtab_entry *f_module_lookup (const char *module_name);

/* Find a given symbol in a Fortran module entry.  */

extern struct symbol *f_module_lookup_symbol (const struct modtab_entry *mte,
                                              const char *symbol_name);

/* Find a non-local Fortran symbol.  */

extern struct symbol *f_lookup_symbol_nonlocal (const char *name,
                                                const struct block *block,
                                                const domain_enum domain);

#endif /* !defined(F_MODULE_H) */
