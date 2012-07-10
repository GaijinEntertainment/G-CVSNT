/* patch - a program to apply diffs to original files */

/* $Id: patch.cpp,v 1.1 2012/03/04 01:06:58 aliot Exp $ */

/* Copyright 1984, 1985-1987, 1988 Larry Wall
   Copyright 1989, 1990-1993, 1997-1998, 1999 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.
   If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#define XTERN
#include <common.h>
#undef XTERN
#define XTERN extern
#include <backupfile.h>
#include <inp.h>
#include <pch.h>
#include <quotearg.h>
#include <util.h>
#include <version.h>
#include <xalloc.h>

#if HAVE_UTIME_H
# include <utime.h>
#endif
#ifdef _MSC_VER
# include <sys/utime.h>
# include <io.h>
#endif

#include <string>

//!!
class ThreadSafeProgress;

/* Some nonstandard hosts don't declare this structure even in <utime.h>.  */
#if ! HAVE_STRUCT_UTIMBUF
struct utimbuf
{
  time_t actime;
  time_t modtime;
};
#endif

/* Output stream state.  */
struct outstate
{
  FILE *ofp;
  int after_newline;
  int zero_output;
};

/* procedures */

static FILE *create_output_file PARAMS ((char const *, int));
static LINENUM locate_hunk(ThreadSafeProgress*, LINENUM);
static bool apply_hunk(ThreadSafeProgress*, struct outstate *, LINENUM);
static bool copy_till(ThreadSafeProgress*, struct outstate *, LINENUM);
static bool patch_match PARAMS ((LINENUM, LINENUM, LINENUM, LINENUM));
static bool similar PARAMS ((char const *, size_t, char const *, size_t));
static bool spew_output(ThreadSafeProgress*, struct outstate *);
static char const *make_temp PARAMS ((int));
static int numeric_string PARAMS ((char const *, int, char const *));
static void abort_hunk PARAMS ((void));
static void cleanup PARAMS ((void));
static void get_some_switches PARAMS ((void));
static void init_output PARAMS ((char const *, int, struct outstate *));
static void init_reject PARAMS ((void));
static void reinitialize_almost_everything PARAMS ((void));
static void remove_if_needed PARAMS ((char const *, int volatile *));
static void usage PARAMS ((FILE *, int)) __attribute__((noreturn));

static int make_backups;
static int backup_if_mismatch;
static char const *version_control;
static char const *version_control_context;
static int remove_empty_files;

/* TRUE if -R was specified on command line.  */
static int reverse_flag_specified;

/* how many input lines have been irretractably output */
static LINENUM last_frozen_line;

static char const *do_defines; /* symbol to patch using ifdef, ifndef, etc. */
static char const if_defined[] = "\n#ifdef %s\n";
static char const not_defined[] = "#ifndef %s\n";
static char const else_defined[] = "\n#else\n";
static char const end_defined[] = "\n#endif /* %s */\n";

static int Argc;
static char * const *Argv;

static FILE *rejfp;  /* reject file pointer */

static char const *patchname;
static char *rejname;
static char const * volatile TMPREJNAME;
static int volatile TMPREJNAME_needs_removal;

static LINENUM last_offset;
static LINENUM maxfuzz = 2;

static char serrbuf[BUFSIZ];

char const program_name[] = "patch";

/* Apply a set of diffs as appropriate. */

bool Patch(ThreadSafeProgress* progress, const char* filename)
{
    patchname = filename;

    verbosity = VERBOSE;
    
    // strippath = numeric_string (optarg, 0, "strip count");
    
    char const *val;
    bool somefailed = FALSE;
    struct outstate outstate;
    char numbuf[LINENUM_LENGTH_BOUND + 1];

    init_time ();

    setbuf(stderr, serrbuf);

    xalloc_fail_func = memory_fatal;
    bufsize = 8 * 1024;
    buf = xmalloc (bufsize);

    strippath = -1;

    posixly_correct = getenv ("POSIXLY_CORRECT") != 0;
    backup_if_mismatch = ! posixly_correct;

    val = getenv ("SIMPLE_BACKUP_SUFFIX");
    if (val && *val)
      simple_backup_suffix = val;

    if ((version_control = getenv ("PATCH_VERSION_CONTROL")))
      version_control_context = "$PATCH_VERSION_CONTROL";
    else if ((version_control = getenv ("VERSION_CONTROL")))
      version_control_context = "$VERSION_CONTROL";

    /* Cons up the names of the global temporary files.
       Do this before `cleanup' can possibly be called (e.g. by `pfatal').  */
    TMPOUTNAME = make_temp ('o');
    TMPINNAME = make_temp ('i');
    TMPREJNAME = make_temp ('r');
    TMPPATNAME = make_temp ('p');

    init_output (outfile, 0, &outstate);

    for (
       open_patch_file (progress, patchname);
       there_is_another_patch(progress);
       reinitialize_almost_everything()
    ) {              /* for each patch in patch file */
       int hunk = 0;
       int failed = 0;
       int mismatch = 0;
       char *outname = outfile ? outfile : inname;

       if (!skip_rest_of_patch)
          get_input_file (progress, inname, outname);

       if (diff_type == ED_DIFF) {
          outstate.zero_output = 0;
          if (! dry_run)
          {
             do_ed_script (progress, outstate.ofp);
             if (! outfile)
             {
                struct stat statbuf;
                if (stat (TMPOUTNAME, &statbuf) != 0)
                   pfatal ("%s", TMPOUTNAME);
                outstate.zero_output = statbuf.st_size == 0;
             }
          }
       } else {
          int got_hunk;
          int apply_anyway = 0;

          /* initialize the patched file */
          if (! skip_rest_of_patch && ! outfile)
          {
             int exclusive = TMPOUTNAME_needs_removal ? 0 : O_EXCL;
             TMPOUTNAME_needs_removal = 1;
             init_output (TMPOUTNAME, exclusive, &outstate);
          }

          /* initialize reject file */
          init_reject ();

          /* find out where all the lines are */
          if (!skip_rest_of_patch)
             scan_input (progress, inname);

          /* from here on, open no standard i/o files, because malloc */
          /* might misfire and we can't catch it easily */

          /* apply each hunk of patch */
          while (0 < (got_hunk = another_hunk (progress, diff_type, reverse))) {
             LINENUM where = 0; /* Pacify `gcc -Wall'.  */
             LINENUM newwhere;
             LINENUM fuzz = 0;
             LINENUM prefix_context = pch_prefix_context ();
             LINENUM suffix_context = pch_suffix_context ();
             LINENUM context = (prefix_context < suffix_context
                                ? suffix_context : prefix_context);
             LINENUM mymaxfuzz = (maxfuzz < context ? maxfuzz : context);
             hunk++;
             if (!skip_rest_of_patch) {
                do {
                   where = locate_hunk(progress, fuzz);
                   if (! where || fuzz || last_offset)
                      mismatch = 1;
                   if (hunk == 1 && ! where && ! (force || apply_anyway)
                       && reverse == reverse_flag_specified) {
                      /* dwim for reversed patch? */
                      if (!pch_swap()) {
                         say (progress, 
                            "Not enough memory to try swapped hunk!  Assuming unswapped.\n");
                         continue;
                      }
                      /* Try again.  */
                      where = locate_hunk (progress, fuzz);
                      if (where
                          && (ok_to_reverse(progress, 
                                            "%s patch detected!",
                                            (reverse
                                             ? "Unreversed"
                                             : "Reversed (or previously applied)"))))
                         reverse ^= 1;
                      else
                      {
                         /* Put it back to normal.  */
                         if (! pch_swap ())
                            fatal ("lost hunk on alloc error!");
                         if (where)
                         {
                            apply_anyway = 1;
                            fuzz--; /* Undo `++fuzz' below.  */
                            where = 0;
                         }
                      }
                   }
                } while (!skip_rest_of_patch && !where
                         && ++fuzz <= mymaxfuzz);
                
                if (skip_rest_of_patch) {     /* just got decided */
                   if (outstate.ofp && ! outfile)
                   {
                      fclose (outstate.ofp);
                      outstate.ofp = 0;
                   }
                }
             }

             newwhere = pch_newfirst() + last_offset;
             if (skip_rest_of_patch) {
                abort_hunk();
                failed++;
                if (verbosity == VERBOSE)
                   say (progress, "Hunk #%d ignored at %s.\n", hunk,
                        format_linenum (numbuf, newwhere));
             }
             else if (!where
                      || (where == 1 && pch_says_nonexistent (reverse) == 2
                          && instat.st_size)) {
                
                if (where)
                   say (progress, "Patch attempted to create file %s, which already exists.\n",
                        quotearg (inname));
                
                abort_hunk();
                failed++;
                if (verbosity != SILENT)
                   say (progress, "Hunk #%d FAILED at %s.\n", hunk,
                        format_linenum (numbuf, newwhere));
             }
             else if (! apply_hunk (progress, &outstate, where)) {
                abort_hunk ();
                failed++;
                if (verbosity != SILENT)
                   say (progress, "Hunk #%d FAILED at %s.\n", hunk,
                        format_linenum (numbuf, newwhere));
             } else {
                if (verbosity == VERBOSE
                    || (verbosity != SILENT && (fuzz || last_offset))) {
                   say (progress, "Hunk #%d succeeded at %s", hunk,
                        format_linenum (numbuf, newwhere));
                   if (fuzz)
                      say (progress, " with fuzz %s", format_linenum (numbuf, fuzz));
                   if (last_offset)
                      say (progress, " (offset %s line%s)",
                           format_linenum (numbuf, last_offset),
                           "s" + (last_offset == 1));
                   say (progress, ".\n");
                }
             }
          }
          
          if (!skip_rest_of_patch)
          {
             if (got_hunk < 0  &&  using_plan_a)
             {
                if (outfile)
                   fatal ("out of memory using Plan A");
                say (progress, "\n\nRan out of memory using Plan A -- trying again...\n\n");
                if (outstate.ofp)
                {
                   fclose (outstate.ofp);
                   outstate.ofp = 0;
                }
                fclose (rejfp);
                continue;
             }

             /* Finish spewing out the new file.  */
             assert (hunk);
             if (! spew_output (progress, &outstate))
             {
                say (progress, "Skipping patch.\n");
                skip_rest_of_patch = TRUE;
             }
          }
       }

       /* and put the output where desired */
       if (! skip_rest_of_patch && ! outfile) {
          if (outstate.zero_output
              && (remove_empty_files
                  || (pch_says_nonexistent (reverse ^ 1) == 2
                      && ! posixly_correct)))
          {
             if (verbosity == VERBOSE)
                say (progress, "Removing file %s%s\n", quotearg (outname),
                     dry_run ? " and any empty ancestor directories" : "");
             if (! dry_run)
             {
                move_file (progress, (char *) 0, (int *) 0, outname, (mode_t) 0,
                           (make_backups
                            || (backup_if_mismatch && (mismatch | failed))));
                removedirs (progress, outname);
             }
          }
          else
          {
             if (! outstate.zero_output
                 && pch_says_nonexistent (reverse ^ 1))
             {
                mismatch = 1;
                if (verbosity != SILENT)
                   say (progress, "File %s is not empty after patch, as expected\n",
                        quotearg (outname));
             }

             if (! dry_run)
             {
                time_t t;
                
                move_file (progress, TMPOUTNAME, &TMPOUTNAME_needs_removal,
                           outname, instat.st_mode,
                           (make_backups
                            || (backup_if_mismatch && (mismatch | failed))));

                if ((set_time | set_utc)
                    && (t = pch_timestamp (reverse ^ 1)) != (time_t) -1)
                {
                   struct utimbuf utimbuf;
                   utimbuf.actime = utimbuf.modtime = t;

                   if (! force && ! inerrno
                       && pch_says_nonexistent (reverse) != 2
                       && (t = pch_timestamp (reverse)) != (time_t) -1
                       && t != instat.st_mtime)
                      say (progress, "Not setting time of file %s (time mismatch)\n",
                           quotearg (outname));
                   else if (! force && (mismatch | failed))
                      say (progress, "Not setting time of file %s (contents mismatch)\n",
                           quotearg (outname));
                   else if (utime (outname, &utimbuf) != 0)
                      pfatal ("Can't set timestamp on file %s",
                              quotearg (outname));
                }
                
                if (! inerrno && chmod (outname, instat.st_mode) != 0)
                   pfatal ("Can't set permissions on file %s",
                           quotearg (outname));
             }
          }
       }
       if (diff_type != ED_DIFF) {
          if (fclose (rejfp) != 0)
             write_fatal ();
          if (failed) {
             somefailed = TRUE;
             say (progress, "%d out of %d hunk%s %s", failed, hunk, "s" + (hunk == 1),
                  skip_rest_of_patch ? "ignored" : "FAILED");
             if (outname) {
                char *rej = rejname;
                if (!rejname) {
                   rej = xmalloc (strlen (outname) + 5);
                   strcpy (rej, outname);
                   addext (rej, ".rej", '#');
                }
                say (progress, " -- saving rejects to file %s", quotearg (rej));
                if (! dry_run)
                {
                   move_file (progress, TMPREJNAME, &TMPREJNAME_needs_removal,
                              rej, instat.st_mode, FALSE);
                   if (! inerrno
                       && (chmod (rej, (instat.st_mode
                                        & ~(S_IXUSR|S_IXGRP|S_IXOTH)))
                           != 0))
                      pfatal ("can't set permissions on file %s",
                              quotearg (rej));
                }
                if (!rejname)
                   free (rej);
             }
             say (progress, "\n");
          }
       }
    }
    if (outstate.ofp && (ferror (outstate.ofp) || fclose (outstate.ofp) != 0))
       write_fatal ();
    cleanup ();
    return !somefailed;
}

/* Prepare to find the next patch to do in the patch file. */

static void
reinitialize_almost_everything (void)
{
    re_patch();
    re_input();

    input_lines = 0;
    last_frozen_line = 0;

    if (inname) {
       free (inname);
       inname = 0;
    }

    last_offset = 0;

    diff_type = NO_DIFF;

    if (revision) {
       free(revision);
       revision = 0;
    }

    reverse = reverse_flag_specified;
    skip_rest_of_patch = FALSE;
}

/* Handle STRING (possibly negative if NEGATIVE_ALLOWED is nonzero)
   of type ARGTYPE_MSGID by converting it to an integer,
   returning the result.  */
static int
numeric_string (char const *string,
                int negative_allowed,
                char const *argtype_msgid)
{
   int value = 0;
   char const *p = string;
   int sign = *p == '-' ? -1 : 1;

   p += *p == '-' || *p == '+';

   do
   {
      int v10 = value * 10;
      int digit = *p - '0';
      int signed_digit = sign * digit;
      int next_value = v10 + signed_digit;

      if (9 < (unsigned) digit)
         fatal ("%s %s is not a number", argtype_msgid, quotearg (string));

      if (v10 / 10 != value || (next_value < v10) != (signed_digit < 0))
         fatal ("%s %s is too large", argtype_msgid, quotearg (string));

      value = next_value;
   }
   while (*++p);

   if (value < 0 && ! negative_allowed)
      fatal ("%s %s is negative", argtype_msgid, quotearg (string));

   return value;
}

/* Attempt to find the right place to apply this hunk of patch. */

static LINENUM
locate_hunk (ThreadSafeProgress* progress, LINENUM fuzz)
{
   register LINENUM first_guess = pch_first () + last_offset;
   register LINENUM offset;
   LINENUM pat_lines = pch_ptrn_lines();
   LINENUM prefix_context = pch_prefix_context ();
   LINENUM suffix_context = pch_suffix_context ();
   LINENUM context = (prefix_context < suffix_context
                      ? suffix_context : prefix_context);
   LINENUM prefix_fuzz = fuzz + prefix_context - context;
   LINENUM suffix_fuzz = fuzz + suffix_context - context;
   LINENUM max_where = input_lines - (pat_lines - suffix_fuzz) + 1;
   LINENUM min_where = last_frozen_line + 1 - (prefix_context - prefix_fuzz);
   LINENUM max_pos_offset = max_where - first_guess;
   LINENUM max_neg_offset = first_guess - min_where;
   LINENUM max_offset = (max_pos_offset < max_neg_offset
                         ? max_neg_offset : max_pos_offset);

   if (!pat_lines)        /* null range matches always */
      return first_guess;

   /* Do not try lines <= 0.  */
   if (first_guess <= max_neg_offset)
      max_neg_offset = first_guess - 1;

   if (prefix_fuzz < 0)
   {
      /* Can only match start of file.  */

      if (suffix_fuzz < 0)
         /* Can only match entire file.  */
         if (pat_lines != input_lines || prefix_context < last_frozen_line)
            return 0;

      offset = 1 - first_guess;
      if (last_frozen_line <= prefix_context
          && offset <= max_pos_offset
          && patch_match (first_guess, offset, (LINENUM) 0, suffix_fuzz))
      {
         last_offset = offset;
         return first_guess + offset;
      }
      else
         return 0;
   }

   if (suffix_fuzz < 0)
   {
      /* Can only match end of file.  */
      offset = first_guess - (input_lines - pat_lines + 1);
      if (offset <= max_neg_offset
          && patch_match (first_guess, -offset, prefix_fuzz, (LINENUM) 0))
      {
         last_offset = - offset;
         return first_guess - offset;
      }
      else
         return 0;
   }

   for (offset = 0;  offset <= max_offset;  offset++) {
      char numbuf0[LINENUM_LENGTH_BOUND + 1];
      char numbuf1[LINENUM_LENGTH_BOUND + 1];
      if (offset <= max_pos_offset
          && patch_match (first_guess, offset, prefix_fuzz, suffix_fuzz)) {
         if (debug & 1)
            say (progress, "Offset changing from %s to %s\n",
                 format_linenum (numbuf0, last_offset),
                 format_linenum (numbuf1, offset));
         last_offset = offset;
         return first_guess+offset;
      }
      if (0 < offset && offset <= max_neg_offset
          && patch_match (first_guess, -offset, prefix_fuzz, suffix_fuzz)) {
         if (debug & 1)
            say (progress, "Offset changing from %s to %s\n",
                 format_linenum (numbuf0, last_offset),
                 format_linenum (numbuf1, -offset));
         last_offset = -offset;
         return first_guess-offset;
      }
   }
   return 0;
}

/* We did not find the pattern, dump out the hunk so they can handle it. */

static void
abort_hunk (void)
{
   register LINENUM i;
   register LINENUM pat_end = pch_end ();
   /* add in last_offset to guess the same as the previous successful hunk */
   LINENUM oldfirst = pch_first() + last_offset;
   LINENUM newfirst = pch_newfirst() + last_offset;
   LINENUM oldlast = oldfirst + pch_ptrn_lines() - 1;
   LINENUM newlast = newfirst + pch_repl_lines() - 1;
   char const *stars =
      (int) NEW_CONTEXT_DIFF <= (int) diff_type ? " ****" : "";
   char const *minuses =
      (int) NEW_CONTEXT_DIFF <= (int) diff_type ? " ----" : " -----";

   fprintf(rejfp, "***************\n");
   for (i=0; i<=pat_end; i++) {
      char numbuf0[LINENUM_LENGTH_BOUND + 1];
      char numbuf1[LINENUM_LENGTH_BOUND + 1];
      switch (pch_char(i)) {
      case '*':
         if (oldlast < oldfirst)
            fprintf(rejfp, "*** 0%s\n", stars);
         else if (oldlast == oldfirst)
            fprintf (rejfp, "*** %s%s\n",
                     format_linenum (numbuf0, oldfirst), stars);
         else
            fprintf (rejfp, "*** %s,%s%s\n",
                     format_linenum (numbuf0, oldfirst),
                     format_linenum (numbuf1, oldlast), stars);
         break;
      case '=':
         if (newlast < newfirst)
            fprintf(rejfp, "--- 0%s\n", minuses);
         else if (newlast == newfirst)
            fprintf (rejfp, "--- %s%s\n",
                     format_linenum (numbuf0, newfirst), minuses);
         else
            fprintf (rejfp, "--- %s,%s%s\n",
                     format_linenum (numbuf0, newfirst),
                     format_linenum (numbuf1, newlast), minuses);
         break;
      case ' ': case '-': case '+': case '!':
         fprintf (rejfp, "%c ", pch_char (i));
         /* fall into */
      case '\n':
         pch_write_line (i, rejfp);
         break;
      default:
         fatal ("fatal internal error in abort_hunk");
      }
      if (ferror (rejfp))
         write_fatal ();
   }
}

/* We found where to apply it (we hope), so do it. */

static bool
apply_hunk (ThreadSafeProgress* progress, struct outstate *outstate, LINENUM where)
{
   register LINENUM old = 1;
   register LINENUM lastline = pch_ptrn_lines ();
   register LINENUM newln = lastline+1;
   register enum {OUTSIDE, IN_IFNDEF, IN_IFDEF, IN_ELSE} def_state = OUTSIDE;
   register char const *R_do_defines = do_defines;
   register LINENUM pat_end = pch_end ();
   register FILE *fp = outstate->ofp;

   where--;
   while (pch_char(newln) == '=' || pch_char(newln) == '\n')
      newln++;

   while (old <= lastline) {
      if (pch_char(old) == '-') {
         assert (outstate->after_newline);
         if (! copy_till (progress, outstate, where + old - 1))
            return FALSE;
         if (R_do_defines) {
            if (def_state == OUTSIDE) {
               fprintf (fp, outstate->after_newline + if_defined,
                        R_do_defines);
               def_state = IN_IFNDEF;
            }
            else if (def_state == IN_IFDEF) {
               fprintf (fp, outstate->after_newline + else_defined);
               def_state = IN_ELSE;
            }
            if (ferror (fp))
               write_fatal ();
            outstate->after_newline = pch_write_line (old, fp);
            outstate->zero_output = 0;
         }
         last_frozen_line++;
         old++;
      }
      else if (newln > pat_end) {
         break;
      }
      else if (pch_char(newln) == '+') {
         if (! copy_till (progress, outstate, where + old - 1))
            return FALSE;
         if (R_do_defines) {
            if (def_state == IN_IFNDEF) {
               fprintf (fp, outstate->after_newline + else_defined);
               def_state = IN_ELSE;
            }
            else if (def_state == OUTSIDE) {
               fprintf (fp, outstate->after_newline + if_defined,
                        R_do_defines);
               def_state = IN_IFDEF;
            }
            if (ferror (fp))
               write_fatal ();
         }
         outstate->after_newline = pch_write_line (newln, fp);
         outstate->zero_output = 0;
         newln++;
      }
      else if (pch_char(newln) != pch_char(old)) {
         char numbuf0[LINENUM_LENGTH_BOUND + 1];
         char numbuf1[LINENUM_LENGTH_BOUND + 1];
         if (debug & 1)
            say (progress, "oldchar = '%c', newchar = '%c'\n",
                 pch_char (old), pch_char (newln));
         fatal ("Out-of-sync patch, lines %s,%s -- mangled text or line numbers, maybe?",
                format_linenum (numbuf0, pch_hunk_beg() + old),
                format_linenum (numbuf1, pch_hunk_beg() + newln));
      }
      else if (pch_char(newln) == '!') {
         assert (outstate->after_newline);
         if (! copy_till (progress, outstate, where + old - 1))
            return FALSE;
         assert (outstate->after_newline);
         if (R_do_defines) {
            fprintf (fp, not_defined, R_do_defines);
            if (ferror (fp))
               write_fatal ();
            def_state = IN_IFNDEF;
         }

         do
         {
            if (R_do_defines) {
               outstate->after_newline = pch_write_line (old, fp);
            }
            last_frozen_line++;
            old++;
         }
         while (pch_char (old) == '!');

         if (R_do_defines) {
            fprintf (fp, outstate->after_newline + else_defined);
            if (ferror (fp))
               write_fatal ();
            def_state = IN_ELSE;
         }

         do
         {
            outstate->after_newline = pch_write_line (newln, fp);
            newln++;
         }
         while (pch_char (newln) == '!');
         outstate->zero_output = 0;
      }
      else {
         assert(pch_char(newln) == ' ');
         old++;
         newln++;
         if (R_do_defines && def_state != OUTSIDE) {
            fprintf (fp, outstate->after_newline + end_defined,
                     R_do_defines);
            if (ferror (fp))
               write_fatal ();
            outstate->after_newline = 1;
            def_state = OUTSIDE;
         }
      }
   }
   if (newln <= pat_end && pch_char(newln) == '+') {
      if (! copy_till (progress, outstate, where + old - 1))
         return FALSE;
      if (R_do_defines) {
         if (def_state == OUTSIDE) {
            fprintf (fp, outstate->after_newline + if_defined,
                     R_do_defines);
            def_state = IN_IFDEF;
         }
         else if (def_state == IN_IFNDEF) {
            fprintf (fp, outstate->after_newline + else_defined);
            def_state = IN_ELSE;
         }
         if (ferror (fp))
            write_fatal ();
         outstate->zero_output = 0;
      }

      do
      {
         if (! outstate->after_newline  &&  putc ('\n', fp) == EOF)
            write_fatal ();
         outstate->after_newline = pch_write_line (newln, fp);
         outstate->zero_output = 0;
         newln++;
      }
      while (newln <= pat_end && pch_char (newln) == '+');
   }
   if (R_do_defines && def_state != OUTSIDE) {
      fprintf (fp, outstate->after_newline + end_defined, R_do_defines);
      if (ferror (fp))
         write_fatal ();
      outstate->after_newline = 1;
   }
   return TRUE;
}

/* Create an output file.  */

static FILE *
create_output_file (char const *name, int open_flags)
{
   int fd = create_file (name, O_WRONLY | binary_transput | open_flags,
                         instat.st_mode);
   FILE *f = fdopen (fd, binary_transput ? "wb" : "w");
   if (! f)
      pfatal ("Can't create file %s", quotearg (name));
   return f;
}

/* Open the new file. */

static void
init_output (char const *name, int open_flags, struct outstate *outstate)
{
   outstate->ofp = name ? create_output_file (name, open_flags) : (FILE *) 0;
   outstate->after_newline = 1;
   outstate->zero_output = 1;
}

/* Open a file to put hunks we can't locate. */

static void
init_reject (void)
{
   int exclusive = TMPREJNAME_needs_removal ? 0 : O_EXCL;
   TMPREJNAME_needs_removal = 1;
   rejfp = create_output_file (TMPREJNAME, exclusive);
}

/* Copy input file to output, up to wherever hunk is to be applied. */

static bool
copy_till (ThreadSafeProgress* progress, register struct outstate *outstate, register LINENUM lastline)
{
   register LINENUM R_last_frozen_line = last_frozen_line;
   register FILE *fp = outstate->ofp;
   register char const *s;
   size_t size;

   if (R_last_frozen_line > lastline)
   {
      say (progress, "misordered hunks! output would be garbled\n");
      return FALSE;
   }
   while (R_last_frozen_line < lastline)
   {
      s = ifetch (++R_last_frozen_line, 0, &size);
      if (size)
      {
         if ((! outstate->after_newline  &&  putc ('\n', fp) == EOF)
             || ! fwrite (s, sizeof *s, size, fp))
            write_fatal ();
         outstate->after_newline = s[size - 1] == '\n';
         outstate->zero_output = 0;
      }
   }
   last_frozen_line = R_last_frozen_line;
   return TRUE;
}

/* Finish copying the input file to the output file. */

static bool
spew_output (ThreadSafeProgress* progress, struct outstate *outstate)
{
   if (debug & 256)
   {
      char numbuf0[LINENUM_LENGTH_BOUND + 1];
      char numbuf1[LINENUM_LENGTH_BOUND + 1];
      say (progress, "il=%s lfl=%s\n",
           format_linenum (numbuf0, input_lines),
           format_linenum (numbuf1, last_frozen_line));
   }

   if (last_frozen_line < input_lines)
      if (! copy_till (progress, outstate, input_lines))
         return FALSE;

   if (outstate->ofp && ! outfile)
   {
      if (fclose (outstate->ofp) != 0)
         write_fatal ();
      outstate->ofp = 0;
   }

   return TRUE;
}

/* Does the patch pattern match at line base+offset? */

static bool
patch_match (LINENUM base, LINENUM offset,
             LINENUM prefix_fuzz, LINENUM suffix_fuzz)
{
   register LINENUM pline = 1 + prefix_fuzz;
   register LINENUM iline;
   register LINENUM pat_lines = pch_ptrn_lines () - suffix_fuzz;
   size_t size;
   register char const *p;

   for (iline=base+offset+prefix_fuzz; pline <= pat_lines; pline++,iline++) {
      p = ifetch (iline, offset >= 0, &size);
      if (canonicalize) {
         if (!similar(p, size,
                      pfetch(pline),
                      pch_line_len(pline) ))
            return FALSE;
      }
      else if (size != pch_line_len (pline)
               || memcmp (p, pfetch (pline), size) != 0)
         return FALSE;
   }
   return TRUE;
}

/* Do two lines match with canonicalized white space? */

static bool
similar (register char const *a, register size_t alen,
         register char const *b, register size_t blen)
{
   /* Ignore presence or absence of trailing newlines.  */
   alen  -=  alen && a[alen - 1] == '\n';
   blen  -=  blen && b[blen - 1] == '\n';

   for (;;)
   {
      if (!blen || (*b == ' ' || *b == '\t'))
      {
         while (blen && (*b == ' ' || *b == '\t'))
            b++, blen--;
         if (alen)
         {
            if (!(*a == ' ' || *a == '\t'))
               return FALSE;
            do a++, alen--;
            while (alen && (*a == ' ' || *a == '\t'));
         }
         if (!alen || !blen)
            return alen == blen;
      }
      else if (!alen || *a++ != *b++)
         return FALSE;
      else
         alen--, blen--;
   }
}

/* Make a temporary file.  */

static char const *
make_temp (int letter)
{
    std::string temppath = GetTemporaryPath();
    char* r = xmalloc (temppath.size() + 10);
    sprintf (r, "%sp%cXXXXXX", temppath.c_str(), letter);
    _mktemp (r);
    if (!*r)
        pfatal ("mktemp");
    return r;
}

static void
remove_if_needed (char const *name, int volatile *needs_removal)
{
   if (*needs_removal)
   {
      unlink (name);
      *needs_removal = 0;
   }
}

static void
cleanup (void)
{
  remove_if_needed (TMPINNAME, &TMPINNAME_needs_removal);
  remove_if_needed (TMPOUTNAME, &TMPOUTNAME_needs_removal);
  remove_if_needed (TMPPATNAME, &TMPPATNAME_needs_removal);
  remove_if_needed (TMPREJNAME, &TMPREJNAME_needs_removal);
}
