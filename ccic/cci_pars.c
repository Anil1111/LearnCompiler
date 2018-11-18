//
// Parser: cci_pars.c
//
#include "cci.h"
#include "cci_prot.h"

// Current token.
Token token;
// Block depth 0: Global, 1: Function, 2: Function Local.
int blkNest = 0;
// Error counter.
int err_ct = 0;
// Temporary table.
SymTbl tmpTb = {"", noId, NON_T, 0, 0, 0, 0};
// Symbol table location of the current function.
SymTbl *fncPt = NULL;
// The last statement in a block.
TknKind last_statement;
// Local variable address.
int localAdrs;

#define START_LocalAdrs 1 * INTSIZE;
#define INT_P(p) (int *)(p)

// For continue/break.
#define LOOP_SIZ 20
struct {
  TknKind stkind;
  int looptop;
  int break_flg;
} loopNest[LOOP_SIZ + 1];
int loopNest_ct = 0;
// For continue/break end.

// For switch.
#define SWTCH_SIZ 10
struct {
  int def_adrs;
  int startCaseList;
} swchNest[SWTCH_SIZ + 1];
int swchNest_ct = 0;

#define CASE_SIZ 100
struct {
  int value;
  int adrs;
} caseList[CASE_SIZ + 1];
int caseList_ct = 0;
// For switch, end.

int compile(char *fname) {
  initChTyp();
  gencode2(CALL, -1);
  gencode1(STOP);

  fileOpen(fname);
  token = nextTkn();
  while (token.kind != EofTkn) {
    switch (token.kind) {
    case Int: case Void:
      set_type();
      set_name();
      if (token.kind == '(') fncDecl(); else varDecl();
      break;
    case Semicolon:
      token = nextTkn();
      break;
    default:
      err_ss("Syntax error", token.text);
      token = nextTkn();
    }
  }
  if (err_ct == 0) backPatch_callAdrs();
  // store static region size to address zero.
  *INT_P(mem_adrs(0)) = mallocG(0);

  if (err_ct > 0) fprintf(stderr, "%d errors were found\n", err_ct);
  return err_ct == 0;
}

void varDecl(void) {
  for (;;) {
    set_aryLen();
    enter(tmpTb, varId);
    // End of declaration.
    if (token.kind != ',') break;
    token = nextTkn();
    set_name();
  }
  token = chk_nextTkn(token, ';');
}

void fncDecl(void) {
  SymTbl *f1;

  localAdrs = START_LocalAdrs;
  f1 = search_name(tmpTb.name);
  if (f1 != NULL && f1->nmKind != fncId && f1->nmKind != protId) {
    err_ss("Duplicate identifier.", f1->name);
    f1 = NULL;
  }

  fncPt = enter(tmpTb, fncId);
  token = nextTkn();
  localTBL_open();

  switch (token.kind) {
  case Void:
    token = nextTkn();
    break;
  case ')':
    break;
  default:
    for (;;) {
      set_type();
      set_name();
      enter(tmpTb, paraId);
      ++(fncPt->args);
      if (token.kind != ',') break;
      token = nextTkn();
    }
  }
  token = chk_nextTkn(token, ')');

  if (token.kind == ';') {
    fncPt->nmKind = protId;
  }
  set_adrs(fncPt);
  if (f1 != NULL) {
    fncChk(f1, fncPt);
  }

  switch (token.kind) {
  case ';':
    token = nextTkn();
    break;
  case '{':
    if (IS_MAIN(fncPt)) {
      set_main();
    }
    fncDecl_begin();
    block(1);
    fncDecl_end();
    break;
  default:
    err_s("Function missing \";\" or \"{\"");
    exit(1);
  }
  localTBL_close(fncPt);
  del_fncTable(f1, fncPt);
  fncPt = NULL;
}

void fncChk(SymTbl *f1, SymTbl *f2) {
  if (f1->nmKind == fncId && f2->nmKind == fncId) {
    err_ss("Duplicate function declaration.", f1->name);
    return;
  }

  if (f1->dtTyp != f2->dtTyp || f1->args != f2->args) {
    err_ss("Function proto-type doesn't match definition.", f1->name);
    return;
  }
  return;
}

void fncDecl_begin(void) {
  int i;
  gencode2(ADBR, 0);
  gencode3(STO, LOCAL_F, 0);
  for (i = fncPt->args; i >= 1; i--) {
    gencode3(STO, LOCAL_F, (fncPt + i)->adrs);
  }
}

void fncDecl_end(void) {
  backPatch(fncPt->adrs, -localAdrs);
  if (last_statement != Return) {
    if (IS_MAIN(fncPt)) {
      gencode2(LDI, 0);
    } else if (fncPt->dtTyp != VOID_T) {
      err_s("This system always needs return at the end of non-void function.");
    }
    backPatch_RET(fncPt->adrs);
    gencode3(LOD, LOCAL_F, 0);
    gencode2(ADBR, localAdrs);
    gencode1(RET);
  }
}

void set_main(void) {
  if (fncPt->dtTyp != INT_T || fncPt->args != 0) {
    err_s("Invalid main function syntax.");
  }
  backPatch(0, fncPt->adrs);
}

void block(int is_func) {
  TknKind kd = Others;

  token = nextTkn();
  ++blkNest;
  if (is_func) {
    while (token.kind == Int) {
      set_type();
      set_name();
      varDecl();
    }
  }

  while (token.kind != '}') {
    kd = token.kind;
    statement();
  }
  last_statement = kd;

  --blkNest;
  token = nextTkn();
}

#define UPLABEL(pos) pos=nextCodeCt()
#define JMP_UP(pos) gencode2(JMP,pos)
#define JPT_UP(pos) gencode2(JPT,pos)
#define JMP_DOWN(pos) pos=gencode2(JMP,0)
#define JPF_DOWN(pos) pos=gencode2(JPF,0)
#define DWNLABEL(pos) backPatch(pos,nextCodeCt())

void statement(void) {
  TknKind kd;
  SymTbl *tp;
  DtType ret_typ = fncPt->dtTyp;
  int i, val;
  int LB_TOP, LB_EXP2, LB_EXP3, LB_BODY, LB_ELSE, LB_END, LB_TBL;

  kd = token.kind;

  if (kd == While || kd == Do || kd == Switch) {
    continue_break_begin(kd);
  }
  switch (kd) {
  case Break:
    if (loopNest_ct == 0) {
      err_s("Invalid break.");
    } else {
      gencode2(JMP, NO_FIX_BREAK_ADR);
      loopNest[loopNest_ct].break_flg = TRUE;
    }
    token = chk_nextTkn(nextTkn(), ';');
    break;
  case Continue:
    gencode2(JMP, get_loopTop());
    token = chk_nextTkn(nextTkn(), ';');
    break;
  case Case:
    token = nextTkn();
    get_const(NULL);
    expr_with_chk(0, ':');
    if (!get_const(&val)) {
      err_s("case argument is not a constant.");
    } else if (swchNest_ct == 0) {
      err_s("No corresponding swtich clause.");
    } else {
      for (i = swchNest[swchNest_ct].startCaseList; i <= caseList_ct; i++) {
        if (caseList[i].value == val) {
          err_s("case argument duplicated");
          break;
        }
      }
      incVar(&caseList_ct, CASE_SIZ, "case clause exceeded %d limit.");
      caseList[caseList_ct].value = val;
      caseList[caseList_ct].adrs = nextCodeCt();
    }
    statement();
    break;
  case Default:
    if (swchNest_ct == 0) {
      err_s("No corresponding switch clause.");
    } else if (swchNest[swchNest_ct].def_adrs != -1) {
      err_s("Duplicate defaults lines.");
    } else {
      swchNest[swchNest_ct].def_adrs = nextCodeCt();
    }
    token = chk_nextTkn(nextTkn(), ':');
    statement();
    break;
  case For:
    token = chk_nextTkn(nextTkn(), '(');
    if (token.kind == ';') {
      token = nextTkn();
    } else {
      expr_with_chk(0, ';');
      remove_val();
    }

    UPLABEL(LB_EXP2);
    if (token.kind == ';') {
      gencode2(LDI, 1);
      token = nextTkn();
    } else {
      expr_with_chk(0, ';');
    }
    JPF_DOWN(LB_END);
    JMP_DOWN(LB_BODY);

    continue_break_begin(kd);
    UPLABEL(LB_EXP3);
    if (token.kind == ')') {
      token = nextTkn();
    } else {
      expr_with_chk(0, ')');
      remove_val();
    }
    JMP_UP(LB_EXP2);

    DWNLABEL(LB_BODY);
    statement();
    JMP_UP(LB_EXP3);

    DWNLABEL(LB_BODY);
    statement();
    JMP_UP(LB_EXP3);

    DWNLABEL(LB_END);
    break;
  case If:
    token = nextTkn();
    expr_with_chk('(', ')');
    JPF_DOWN(LB_ELSE);
    statement();
    if (token.kind != Else) {
      DWNLABEL(LB_ELSE);
      break;
    }
    JMP_DOWN(LB_END);
    DWNLABEL(LB_ELSE);
    token = nextTkn();
    statement();
    DWNLABEL(LB_END);
    break;
  case While:
    token = nextTkn();
    UPLABEL(LB_TOP);
    expr_with_chk('(', ')');
    JPF_DOWN(LB_END);
    statement();
    JMP_UP(LB_TOP);
    DWNLABEL(LB_END);
    break;
  case Do:
    token = nextTkn();
    UPLABEL(LB_TOP);
    statement();
    if (token.kind == While) {
      token = nextTkn();
      expr_with_chk('(', ')');
      token = chk_nextTkn(token, ';');
      JPT_UP(LB_TOP);
    } else {
      err_s("\"do\" missing while at the end.");
    }
    break;
  case Switch:
    token = nextTkn();
    expr_with_chk('(', ')');
    JMP_DOWN(LB_TBL);
    swch_begin();
    statement();
    JMP_DOWN(LB_END);
    DWNLABEL(LB_TBL);
    swch_end();
    DWNLABEL(LB_END);
    break;
  case Return:
    token = nextTkn();
    if (token.kind == ';') {
      if (ret_typ != VOID_T) {
        err_s("\"return\" sentence missing return value.");
      } else {
        expression();
        if (ret_typ == VOID_T) {
          err_s("\"void\" is returning a value.");
        }
      }
    }
    gencode2(JMP, NO_FIX_RET_ADR);
    token = chk_nextTkn(token, ';');
    break;
  case Printf:
  case Exit:
    sys_fncCall(kd);
    token = chk_nextTkn(token, ';');
    break;
  case Input:
    expr_with_chk(0, ';');
    remove_val();
    break;
  case Incre:
  case Decre:
    expr_with_chk(0, ';');
    remove_val();
    break;
  case Ident:
    tp = search(token.text);
    if ((tp->nmKind == fncId || tp->nmKind == protId) && tp->dtTyp == VOID_T) {
      fncCall(tp);
      token = chk_nextTkn(token, ';');
    } else {
      expr_with_chk(0, ';');
      remove_val();
    }
    break;
  case Lbrace:
    block(0);
    break;
  case Semicolon:
    token = nextTkn();
    break;
  case EofTkn:
    err_s("Unexpected end of the code.");
    exit(1);
  default:
    err_ss("Invalid description.", token.text);
    token = nextTkn();
  }
  if (kd == For || kd == While || kd == Do || kd == Switch) {
    continue_break_end();
  }
}

void continue_break_begin(TknKind stmnt) {
  incVar(&loopNest_ct, LOOP_SIZ, "Control nest exceeded %d limit.");
  loopNest[loopNest_ct].stkind = stmnt;
  loopNest[loopNest_ct].looptop = nextCodeCt();
  loopNest[loopNest_ct].break_flg = FALSE;
}

void continue_break_end(void) {
  if (loopNest[loopNest_ct].break_flg) {
    backPatch_BREAK(loopNest[loopNest_ct].looptop);
  }
  --loopNest_ct;
}

int get_loopTop(void) {
  int i;
  for (i = loopNest_ct; i > 0; i--) {
    if (loopNest[i].stkind != Switch) {
      return loopNest[i].looptop;
    }
  }
  err_s("\"continue\" appears outside of a loop.");
  return 0;
}

void swch_begin(void) {
  incVar(&swchNest_ct, SWTCH_SIZ, "\"switch\" nest exceed %d limit.");
  swchNest[swchNest_ct].def_adrs = -1;
  swchNest[swchNest_ct].startCaseList = caseList_ct + 1;
}

void swch_end(void) {
  int i, start = swchNest[swchNest_ct].startCaseList;

  for (i = start; i <= caseList_ct; i++) {
    gencode2(EQCMP, caseList[i].value);
    gencode2(JPT, caseList[i].adrs);
  }
  gencode1(DEL);
  if (swchNest[swchNest_ct].def_adrs != -1) {
    gencode2(JMP, swchNest[swchNest_ct].def_adrs);
  }
  caseList_ct = start - 1;
  --swchNest_ct;
}

void expr_with_chk(TknKind k1, TknKind k2) {
  if (k1 != 0) {
    token = chk_nextTkn(token, k1);
  }
  expression();
  if (k2 != 0) {
    token = chk_nextTkn(token, k2);
  }
}

void expression(void) {
  term(2);
  if (token.kind == '=') {
    to_leftVal();
    token = nextTkn();
    expression();
    gencode1(ASSV);
  }
}

void term(int n) {
  TknKind kd;

  if (n == 8) {
    factor();
    return;
  }
  term(n+1);
  while (n == opOrder(token.kind)) {
    kd = token.kind;
    token = nextTkn();
    term(n + 1);
    gencode_Binary(kd);
  }
}

void factor(void) {
  SymTbl *tp;
  TknKind kd = token.kind;

  switch (kd) {
  case Plus:
  case Minus:
  case Not:
  case Incre:
  case Decre:
    token = nextTkn();
    factor();
    if (kd == Incre || kd == Decre) {
      to_leftVal();
    }
    gencode_Unary(kd);
    break;
  case IntNum:
    gencode2(LDI, token.intVal);
    token = nextTkn();
    break;
  case Lparen:
    expr_with_chk('(', ')');
    break;
  case Printf:
  case Input:
  case Exit:
    if (kd != Input) {
      err_ss("\"void\" function is used inside an expression.", token.text);
    }
    sys_fncCall(kd);
    break;
  case Ident:
    tp = search(token.text);
    switch (tp->nmKind) {
    case fncId:
    case protId:
      if (tp->dtTyp == VOID_T) {
      err_ss("\"void\" function is used inside an expression.", tp->name);
      }
      fncCall(tp);
      break;
    case varId:
    case paraId:
      if (tp->aryLen == 0) {
        gencode3(LOD, b_flg(tp), tp->adrs);
        token = nextTkn();
      } else {
        token = nextTkn();
        if (token.kind == '[') {
          gencode3(LDA, b_flg(tp), tp->adrs);
          expr_with_chk('[', ']');
          gencode2(LDI, INTSIZE);
          gencode1(MUL);
          gencode1(ADD);
          gencode1(VAL);
        } else {
          err_s("No array index.");
        }
      }
      if (token.kind == Incre || token.kind == Decre) {
        to_leftVal();
        if (token.kind == Incre) {
          gencode1(INC);
          gencode2(LDI, 1);
          gencode1(SUB);
        } else {
          gencode1(DEC);
          gencode2(LDI, 1);
          gencode1(ADD);
        }
        token = nextTkn();
      }
      break;
    }
    break;
  default:
    err_ss("Invalid description.", token.text);
  } // switch(kd)
}

void fncCall(SymTbl *fp) {
  int argCt = 0;

  token = chk_nextTkn(nextTkn(), '(');
  if (token.kind != ')') {
    for (;;) {
      expression();
      ++argCt;
      if (token.kind != ',') {
        break;
      }
      token = nextTkn();
    }
  }
  token = chk_nextTkn(token, ')');

  if (argCt != fp->args) {
    err_ss("The number of arguments is wrong.", fp->name);
  }
  gencode2(CALL, fp->adrs);
}

void sys_fncCall(TknKind kd) {
  int fnc_typ = 0;
  char *form;

  token = chk_nextTkn(nextTkn(), '(');
  switch (kd) {
  case Exit:
    fnc_typ = EXIT_F;
    expression();
    break;
  case Input:
    fnc_typ = INPUT_F;
    break;
  case Printf:
    if (token.kind != String) {
      err_s("The 1st argument of printf is wrong.");
    }
    gencode2(LDI, token.intVal);
    form = mem_adrs(token.intVal);
    token = nextTkn();
    if (token.kind != ',') {
      fnc_typ = PRINTF1_F;
    } else {
      fnc_typ = PRINTF2_F;
      token = nextTkn();
      expression();
      if (token.kind == ',') {
        err_s("More than 2 arguments for \"printf\"");
      }
      if (!good_format(form)) {
        err_s("Invalid printf format");
      }
    }
    break;
  default:
    ;
  }
  token = chk_nextTkn(token, ')');
  gencode2(LIB, fnc_typ);
}

void set_type(void) {
  tmpTb.aryLen = tmpTb.adrs = tmpTb.args = 0;
  tmpTb.level = blkNest;
  switch (token.kind) {
  case Int:
    tmpTb.dtTyp = INT_T;
    break;
  case Void:
    tmpTb.dtTyp = VOID_T;
    break;
  default:
    err_ss("Invalid type specification.", token.text);
    tmpTb.dtTyp = INT_T;
    
  }
  token = nextTkn();
}

void set_name(void) {
  if (token.kind == Ident) {
    tmpTb.name = s_malloc(token.text);
    token = nextTkn();
  } else {
    err_ss("Invalid description", token.text);
    // Put an impossible name as a temporary name;
    tmpTb.name = "tmp$nane1";
  }
}

void set_aryLen(void) {
  tmpTb.aryLen = 0;
  if (token.kind != '[') {
    return;
  }
  token = nextTkn();
  if (token.kind == ']') {
    err_s("No array index specified.");
    token = nextTkn();
    // Put 1 as a temporary recovery value.
    tmpTb.aryLen = 1;
    return;
  }
  get_const(NULL);
  expr_with_chk(0, ']');
  if (get_const(&(tmpTb.aryLen))) {
    if (tmpTb.aryLen <= 0) {
      tmpTb.aryLen = 1;
      err_s("Invalid array index.");
    }
  } else {
    err_s("Array index is not an integer.");
  }

  if (token.kind == '[') {
    err_ss("Multi-dimensional array is not allowed.", token.text);
  }
}

void set_adrs(SymTbl *tp) {
  int i, size = INTSIZE;

  switch (tp->nmKind) {
  case varId:
    if (tp->aryLen > 0) {
      size *= tp->aryLen;
    }
    if (is_global()) {
      tp->adrs = mallocG(size);
    } else {
      tp->adrs = mallocL(size);
    }
    break;
  case fncId:
    tp->adrs = nextCodeCt();
    for (i = 1; i <= tp->args; i++) {
      (tp+i)->adrs = mallocL(size);
    }
  }
}

int opOrder(TknKind kd) {
  switch(kd) {
  case Multi:
  case Divi:
  case Mod:
    return 7;
  case Plus:
  case Minus:
    return 6;
  case Less:
  case LessEq:
  case Great:
  case GreatEq:
    return 5;
  case Equal:
  case NotEq:
    return 4;
  case And:
    return 3;
  case Or:
    return 2;
  case Assign:
    return 1;
  default:
    return 0;
  }
}

int is_global(void) {
  return (blkNest == 0);
}

int mallocL(int size) {
  if (size < 0) {
    size = 0;
  }
  localAdrs += size;
  return localAdrs - size;
}

int good_format(char *form) {
  char *p;

  // mask %% pattern
  while ((p = strstr(form, "%%")) != NULL) {
    *p = *(p + 1) = '\1';
  }
  if ((p = strchr(form, '%')) == NULL) {
    return FALSE;
  }
  ++p;
  // left align
  if (*p == '-') {
    ++p;
  }
  while (isdigit(*p)) {
    ++p;
  }
  if (*p == '\0') {
    return FALSE;
  }
  if (strchr("cdioxX", *p) == NULL) {
    return FALSE;
  }
  if (strchr(p, '%') != NULL) {
    return FALSE;
  }
  for (p = form; *p; p++) {
    if (*p == '\1') {
      *p = '%';
    }
  }
  return TRUE;
}
