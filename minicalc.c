//
// Calculator minicalc.c
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef enum {
  Print, Lparen, Rparen, Plus, Minus, Multi, Divi,
  Assign, VarName, IntNum, EofTkn, Others
} Kind;

typedef struct {
  Kind kind;
  int val;
} Token;

void input(void);
void statement(void);
void expression(void);
void term(int n);
void factor(void);
Token nextTkn(void);
Token lookTkn(void);
int nextCh(void);
void operate(Kind op);
void push(int n);
int pop(void);
void chkTkn(Kind kd);
int opOrder(Kind kd);

#define STK_SIZ 20
int stack[STK_SIZ + 1];
int stkct;
Token token;
char buf[80], *bufp;
int ch;
int var[26];
int errF;

int main(void) {
  while (1) {
    input();
    token = nextTkn();
    if (token.kind == EofTkn) {
      exit(1);
    }
    statement();
    if (errF) {
      puts("  --err--");
    }
  }
  return 0;
}

void input(void) {
  errF = 0;
  stkct = 0;
  buf[0] = '\0';
  fgets(buf, 80, stdin);
  bufp = buf;
  ch = nextCh();
}

void statement(void) {
  switch (token.kind) {
  case VarName:
    expression();
    (void) pop();
    break;
  case Print:
    token = nextTkn();
    expression();
    chkTkn(EofTkn);
    if (errF) {
      break;
    }
    printf("  %d\n", pop());
    return;
  default:
    errF = 1;
  }
  chkTkn(EofTkn);
}

void expression() {
  int vNbr = 0;
  
  if (token.kind == VarName && lookTkn().kind == Assign) {
    vNbr = token.val;
    token = nextTkn();
    token = nextTkn(); // read twice
    expression();
    var[vNbr] = stack[stkct];
  } else {
    term(2);
  }
}


// n: priority
void term(int n) {
  Kind op;

  if (n == 8) {
    factor();
    return;
  }
  term(n + 1);
  while (n == opOrder(token.kind)) {
    op = token.kind;
    token = nextTkn();
    term(n + 1);
    operate(op);
  }
}

// priority of 2-term operators
int opOrder(Kind kd) {
  switch(kd) {
  case Multi:
  case Divi:
  // case Mod:
    return 7;
  case Plus:
  case Minus:
    return 6;
  /* case Less: */
  /* case LessEq: */
  /* case Great: */
  /* case GreatEq: */
  /*   return 5; */
  /* case Equal: */
  /* case NotEq: */
  /*   return 4; */
  /* case And: */
  /* case Or: */
  /*   return 2; */
  case Assign:
    return 1;
  default:
    return 0;
  }
    
}

void factor(void) {
  switch (token.kind) {
  case VarName:
    push(var[token.val]);
    break;
  case IntNum:
    push(token.val);
    break;
  case Lparen:
    token = nextTkn();
    expression();
    chkTkn(Rparen);
    break;
  default:
    errF = 1;
  }
  token = nextTkn();
}

Token nextTkn(void) {
  int num;
  Token tk = {Others, 0};

  while (isspace(ch)) {
    ch = nextCh();
  }
  if (isdigit(ch)) {
    for (num = 0; isdigit(ch); ch = nextCh()) {
      num = num * 10 + (ch - '0');
    }
    tk.val = num;
    tk.kind = IntNum;
  } else if (islower(ch)) {
    tk.val = ch - 'a';
    tk.kind = VarName;
    ch = nextCh();
  } else {
    switch (ch) {
    case '(':
      tk.kind = Lparen;
      break;
    case ')':
      tk.kind = Rparen;
      break;
    case '+':
      tk.kind = Plus;
      break;
    case '-':
      tk.kind = Minus;
      break;
    case '*':
      tk.kind = Multi;
      break;
    case '/':
      tk.kind = Divi;
      break;
    case '=':
      tk.kind = Assign;
      break;
    case '?':
      tk.kind = Print;
      break;
    case '\0':
      tk.kind = EofTkn;
      break;
    }
    ch = nextCh();
  }
  return tk;
}

Token lookTkn(void) {
  char *bufp_back = bufp;
  int ch_bak = ch;
  Token tk = nextTkn();
  bufp = bufp_back;
  ch = ch_bak;
  return tk;
}

int nextCh(void) {
  if (*bufp == '\0') {
    return '\0';
  } else {
    return *bufp++;
  }
}

void operate(Kind op) {
  int d2 = pop();
  int d1 = pop();

  if (op == Divi && d2 == 0)  {
    puts("  division by 0");
    errF = 1;
  }
  if (errF) {
    return;
  }
  switch (op) {
  case Plus:
    push(d1 + d2);
    break;
  case Minus:
    push(d1 - d2);
    break;
  case Multi:
    push(d1 * d2);
    break;
  case Divi:
    push(d1 / d2);
    break;
  default:
    puts("  Invalid operation");
    return;
  }
}

void push(int n) {
  if (errF) {
    return;
  }
  if (stkct + 1 > STK_SIZ) {
    puts("Stack overflow.");
    exit(1);
  }
  stack[++stkct] = n;
}

int pop(void) {
  if (errF) {
    return 1;
  }
  if (stkct < 1) {
    puts("stack underflow.");
    exit(1);
  }
  return stack[stkct--];
}

void chkTkn(Kind kd) {
  if (token.kind != kd) {
    errF = 1;
  }
}
