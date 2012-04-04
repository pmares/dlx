#define _GNU_SOURCE
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dlx.h"
#include "darray.h"

#define F(i, n) for(int i=0; i<n; i++)

#define NORETURN __attribute__((__noreturn__))
void die(const char *err, ...) NORETURN __attribute__((format (printf, 1, 2)));
void die(const char *err, ...) {
  va_list params;
  va_start(params, err);
  vfprintf(stderr, err, params);
  fputc('\n', stderr);
  va_end(params);
  exit(1);
}

void fword(char *s, void f(char *)) {
  for(;;) {
    char *e = strchr(s, ' ');
    if (e) *e = 0;
    f(s);
    if (!e) break;
    s = e + 1;
  }
}

char *mallocgets() {
    char *s = 0;
    size_t len = 0;
    if (-1 == getline(&s, &len, stdin)) return 0;
    // Assumes newline before EOF.
    s[strlen(s) - 1] = 0;
    return s;
}

struct hint_s {
  char cmd;
  int i[8], j[8], k[8], n;
};
typedef struct hint_s *hint_ptr;

darray_ptr hints;

int main() {
  int dim = 0, m = 0;
  char *sym[16][16];
  hints = darray_new();

  for(char *s = 0; (s = mallocgets()) && strcmp(s, "%%"); free(s)) {
    int i = 0;
    void f(char *s) {
      F(i, dim) F(j, m) if (!strcmp(sym[i][j], s)) die("duplicate symbol: %s", s);
      F(j, i) if (!strcmp(sym[dim][j], s)) die("duplicate symbol: %s", s);
      sym[dim][i++] = strdup(s);
    }
    fword(s, f);
    if (!dim) m = i; else if (m != i) die("line %d: wrong number of fields", dim + 1);
    dim++;
  }
  for(char *s = 0; (s = mallocgets()); free(s)) {
    hint_ptr h = 0;
    void f(char *s) {
      if (!h) {
        h = malloc(sizeof(*h));
        h->cmd = *s;
        h->n = 0;
        darray_append(hints, h);
        return;
      }
      h->k[h->n] = 0;
      char *d = strchr(s, '.');
      if (d) {
        *d = 0;
        h->k[h->n] = atoi(d+1);
      }
      F(i, dim) F(j, m) if (!strcmp(sym[i][j], s)) {
        h->i[h->n] = i;
        h->j[h->n] = j;
        h->n++;
        return;
      }
      die("invalid symbol: %s", s);
    }
    fword(s, f);
  }

  darray_ptr con = darray_new();
  darray_ptr choice = darray_new();
  dlx_t dlx = dlx_new();

  void constrain(char *s) {
    darray_append(con, s);
    dlx_add_col(dlx);
  }
  void constrain_opt(char *s) {
printf("opt: %s\n", s);
    dlx_add_col(dlx);
    dlx_mark_optional(dlx, darray_count(con));
    darray_append(con, s);
  }
  void add_1(char *s) {
    F(c, darray_count(con)) if (!strcmp(s, con->item[c])) {
      dlx_set_last_row(dlx, c);
      return;
    }
    die("no such constraint %s", s);
  }
  void new_row(char *s) {
printf("nr: %s\n", s);
    darray_append(choice, s);
    dlx_add_row(dlx);
  }

  F(j, dim) F(k, m) constrain(sym[j][k]);
  char buf[1024] = { 0 };
  int a[m];

  char *constraint_print(int n) { return con->item[n]; }
  dlx_set_col_id(dlx, constraint_print);

  char *hsym(hint_ptr p, int n) { return sym[p->i[n]][p->j[n]]; }
  void optcol(void *data) {
    hint_ptr p = data;
    switch(p->cmd) {
      char *s;
      // TODO: Below, I should encode p->k[0] too, but no puzzles seem to use
      // anything except 0 anyway.
      case '>':
        if (p->n != 2) die("inequality must have exactly 2 fields");
        p->cmd = '<';
        {
           int tmp;
           tmp = p->i[0]; p->i[0] = p->i[1], p->i[1] = tmp;
           tmp = p->j[0]; p->j[0] = p->j[1], p->j[1] = tmp;
           tmp = p->k[0]; p->k[0] = p->k[1], p->k[1] = tmp;
        }
        // FALLT?HROUGH
      case '<':
      case '1':
      case 'A':
        F(k, m) {
          asprintf(&s, "%s%c%s:%d", hsym(p, 0), p->cmd, hsym(p, 1), k);
          constrain_opt(s);
        }
        break;
    }
  }
  darray_forall(hints, optcol);

  void f(char *s, int i) {
    int has(hint_ptr p, int i) {return a[p->i[i]] == p->j[i]; }
    int match(hint_ptr p) {
      int t = 0;
      F(n, p->n) t += has(p, n);
      return t;
    }
    if (i == dim) {
      int anon(void *data) {
        hint_ptr p = data;
        switch(p->cmd) {
          case 'p': return has(p, 0) && has(p, 1) ||
            has(p, 2) && has(p, 3) || (match(p) | 2) != 2;
          case '=': return match(p) == 1;
          case '<':
          case '1':
          case 'A':
          case '!': return match(p) > 1;
          case 'i': return has(p, 0) && (match(p) | 2) != 2;
          /*
          case '^': return matchcount(p) >= 2;
          case '1': return attr(p, 0) + 1 != attr(p, 1);
          case 'A': return abs(attr(p, 0) - attr(p, 1)) != 1;
        */
        }
        return 0;
      }
      if (darray_until(hints, anon)) return;
      new_row(buf);
      F(k, dim) add_1(sym[k][a[k]]);
      void opthints(void *data) {
        hint_ptr p = data;
        switch(p->cmd) {
          char *s;
          case '1':
            if (has(p, 0)) {
              F(k, m) {
                if (k == a[p->k[0]] + 1) continue;
                asprintf(&s, "%s%c%s:%d", hsym(p, 0), p->cmd, hsym(p, 1), k);
                printf(" (opt : %s)", s);
                add_1(s), free(s);
              }
            }
            if (has(p, 1)) {
              asprintf(&s, "%s%c%s:%d", hsym(p, 0), p->cmd, hsym(p, 1), a[p->k[0]]);
              printf(" (opt : %s)", s);
              add_1(s), free(s);
            }
            break;
          case 'A':
            if (has(p, 0)) {
              F(k, m) {
                if (abs(k - a[p->k[0]]) == 1) continue;
                asprintf(&s, "%s%c%s:%d", hsym(p, 0), p->cmd, hsym(p, 1), k);
                printf(" (opt : %s)", s);
                add_1(s), free(s);
              }
            }
            if (has(p, 1)) {
              asprintf(&s, "%s%c%s:%d", hsym(p, 0), p->cmd, hsym(p, 1), a[p->k[0]]);
              printf(" (opt : %s)", s);
              add_1(s), free(s);
            }
            break;
          case '<':
            if (has(p, 0)) {
              for(int k = 0; k <= a[p->k[0]]; k++) {
                asprintf(&s, "%s%c%s:%d", hsym(p, 0), p->cmd, hsym(p, 1), k);
                printf(" (opt : %s)", s);
                add_1(s), free(s);
              }
            }
            if (has(p, 1)) {
              for(int k = a[p->k[1]]; k < m; k++) {
                asprintf(&s, "%s%c%s:%d", hsym(p, 0), p->cmd, hsym(p, 1), k);
                printf(" (opt : %s)", s);
                add_1(s), free(s);
              }
            }
            break;
        }
      }
      darray_forall(hints, opthints);
      return;
    }
    if (i) *s++ = ' ';
    F(k, m) {
      a[i] = k;
      strcpy(s, sym[i][k]);
      f(s + strlen(sym[i][k]), i+1);
    }
  }
  f(buf, 0);

  dlx_search(dlx);
  return 0;
}
