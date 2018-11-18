//
// Process Symbol Table: cci_tbl.c
//
#include "cci.h"
#include "cci_prot.h"

#define TBL_MAX 1000
// Global table starting position
#define start_GTBL 1
// Remaining local table locations
#define LTBL_EMPTY 9999
SymTbl table[TBL_MAX + 1];
// Local table starting position
int start_LTBL = LTBL_EMPTY;
int tableCt = 0;

SymTbl *enter(SymTbl tb, SymKind kind) {
  int tbNo;

  if ((kind == varId || kind == paraId) && tb.dtTyp == VOID_T) {
    err_s("Variable type error (void)");
    tb.dtTyp = INT_T;
  }
  tb.nmKind = kind;
  nameChk(tb);

  if (tableCt >= TBL_MAX) {
    err_s("Smbol table overflow");
    exit(1);
  }
  tbNo = ++tableCt;
  table[tbNo] = tb;

  if (kind == paraId) {
    ++table[tbNo].level;
  }
  if (kind == varId) {
    set_adrs(&table[tbNo]);
  }
  if (kind == fncId) {
    // Negative means, it's only temporary.
    table[tbNo].adrs = -tbNo;
  }

  return &table[tbNo];
}

void localTBL_open(void) {
  start_LTBL = tableCt + 1;
}

void localTBL_close(SymTbl *fp) {
  tableCt = start_LTBL - 1 + fp->args;
  start_LTBL = LTBL_EMPTY;
}

SymTbl *search(char *s) {
  SymTbl *p;
  static SymTbl *dmy_p = NULL, wkTb = {"tmp$name2", varId, INT_T, 0, 0, 0, 0};

  p = search_name(s);
  if (p == NULL) {
    err_ss("Undefined identifile", s);
    if (dmy_p == NULL) {
      dmy_p = enter(wkTb, varId);
    }
    p = dmy_p;
  }
  return p;
}

SymTbl *search_name(char *s) {
  int i;
  for (i = tableCt; i >= start_LTBL; i--) {
    if (strcmp(table[i].name, s) == 0) {
      return table + i;
    }
  }
  for (; i >= start_GTBL; i--) {
    if (table[i].nmKind != paraId && strcmp(table[i].name, s) == 0) {
      return table + i;
    }
  }
  return NULL;
}

void nameChk(SymTbl tb) {
  SymTbl *p;
  extern int blkNest;
  int nest = blkNest;

  if (tb.nmKind != paraId && tb.nmKind != varId) {
    return;
  }
  if ((p = search_name(tb.name)) == NULL) {
    return;
  }
  if (tb.nmKind == paraId) {
    ++nest;
  }
  if (p->level >= nest) {
    err_ss("Duplicate identifier.", tb.name);
  }
}

void del_fncTable(SymTbl *f1, SymTbl *f2) {
  int i;
  if (f1 == NULL) {
    return;
  }
  if (f1->dtTyp != f2->dtTyp || f1->args != f2->args) {
    return;
  }
  if (f1->nmKind == protId && f2->nmKind == fncId) {
    for (i = 0; i <= f2->args; i++) {
      *(f1 + i) = *(f2 + i);
    }
  }
  tableCt -= (f2->args + 1);
  return;
}

int b_flg(SymTbl *tp) {
  if (tp->level == 0) {
    return 0;
  } else {
    return 1;
  }
}

SymTbl *tb_ptr(int n) {
  return &table[n];
}
