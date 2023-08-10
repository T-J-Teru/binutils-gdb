/* Copyright (C) 2023 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef REMOTE_MACHINE_ID_H
#define REMOTE_MACHINE_ID_H

#include <memory>
#include <unordered_map>

/* A base class from which machine-id validation objects can be created.
   A remote target can send GDB a machine-id, which can be used to check
   if the remote target and GDB are running on the same machine, and have
   a common view of the file-system.  Knowing this allows GDB to optimise
   some of its interactions with the remote target.

   A machine-id consists of a set of key-value pairs, where both keys and
   values are std::string objects.  A machine-id has a single master key
   and some number of secondary keys.

   Within GDB the native target will register one or more of these objects
   by calling register_machine_id_validation.  When GDB receives a
   machine-id from a remote-target each registered machine_id_validation
   object will be checked in turn to see if it matches the machined-id.
   If any machine_id_validation matches then this indicates that GDB and
   the remote target are on the same machine.  */
struct machine_id_validation
{
  /* Constructor.  MASTER_KEY is the name of the master key that this
     machine_id_validation object validates for.  */
  machine_id_validation (std::string &&master_key)
    : m_master_key (master_key)
  { /* Nothing.  */ }

  /* Destructor.  */
  virtual ~machine_id_validation ()
  { /* Nothing.  */ }

  /* Return a reference to the master key.  */
  const std::string &
  master_key () const
  {
    return m_master_key;
  }

  /* VALUE is a string passed from the remote target corresponding to the
     key for master_key().  If the remote target didn't pass a key
     matching master_key() then this function should not be called.

     Return true if VALUE matches the value calculated for the host on
     which GDB is currently running.  */
  virtual bool
  check_master_key (const std::string &value) = 0;

  /* This function will only be called for a machine-id which contains a
     key matching master_key(), and for which check_master_key() returned
     true.

     KEY and VALUE are a key-value pair passed from the remote target.
     This function should return true if KEY is known, and VALUE matches
     the value calculated for the host on which GDB is running.  If KEY is
     not known, or VALUE doesn't match, then this function should return
     false.  */
  virtual bool
  check_secondary_key (const std::string &key, const std::string &value) = 0;

private:
  /* The master key for which this object validates machine-ids.  */
  std::string m_master_key;
};

/* Register a new machine-id.  */

extern void register_machine_id_validation
  (std::unique_ptr<machine_id_validation> &&validation);

/* KV_PAIRS contains all machine-id obtained from the remote target, the
   keys are the index into the map, and the values are the values of the
   map.  These pairs are checked against all of the registered
   machine_id_validation objects.

   If any machine_id_validation matches all the data in KV_PAIRS then this
   function returns true, otherwise, this function returns false.

   For KV_PAIRS to match against a machine_id_validation object, KV_PAIRS
   must contain a key matching machine_id_validation::master_key(), and the
   value for that key must return true when passed to the function
   machine_id_validation::check_master_key().  Then, for every other
   key/value pair machine_id_validation::check_secondary_key() must return
   true.  */

extern bool validate_machine_id
  (const std::unordered_map<std::string, std::string> &kv_pairs);

#endif /* REMOTE_MACHINE_ID_H */
