//
// Reverse Polish Notation, polish_p.c
//
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

void polish(char *s);
int execute(void);
int getvalue(int ch);
int order(int ch);
void push(int n);
int pop(void);

#define STK_SIZ 20
int stack[STK_SIZ + 1];
int stkct;
char plsh_out[80];

int main(void) {
  char siki[80];
  int ans;

  printf("Input: ");
  fgets(siki, 80, stdin);
  polish(siki);
  if (plsh_out[0] == '\n') {
    exit(1);
  }
  ans = execute();
  printf("Converted: %s\n", plsh_out);
  printf("Result: %d\n", ans);

  return 0;
}

void polish(char *s) {
  int wk;
  char *out = plsh_out;
  
  stkct = 0;
  while (1) {
    while (isspace(*s)) {
      ++s;
    }
    if (*s == '\0') {
      while (stkct > 0) {
        if ((*out++ = pop()) == '(') {
          puts("Invalid '('");
          exit(1);
        }
      }
      break;
    }
    if (islower(*s) || isdigit(*s)) {
      *out++ = *s++;
      continue;
    }
    switch (*s) {
    case '(':
      push(*s);
      break;
    case ')':
      while ((wk = pop()) != '(') {
        *out++ = wk;
        if (stkct == 0) {
          puts("Missing a '('.\n");
          exit(1);
        }
      }
      break;
    default:
      if (order(*s) == -1) {
        printf("Invalid letter (%c)\n", *s);
        exit(1);
      }
      while (stkct > 0 && (order(stack[stkct]) >= order(*s))) {
        *out++ = pop();
      }
      push(*s);
    }
    s++;
  }
  *out = '\0';
}

int execute(void) {
  int d1, d2;
  char *s;

  stkct = 0;
  for (s = plsh_out; *s; s++) {
    if (islower(*s)) {
      push(getvalue(*s));
    } else if (isdigit(*s)) {
      push(*s - '0');
    } else {
      d2 = pop();
      d1 = pop();
      switch (*s) {
      case '+':
        push(d1 + d2);
        break;
      case '-':
        push(d1 - d2);
        break;
      case '*':
        push(d1 * d2);
        break;
      case '/':
        if (d2 == 0) {
          puts("Divided by zero error.");
          exit(1);
        }
        push(d1 / d2);
        break;
      }
    }
  }
  if (stkct != 1) {
    printf("Error\n");
    exit(1);
  }
  return pop();
}

int getvalue(int ch) {
  if (islower(ch)) {
    return ch - 'a' + 1;
  } else {
    return 0;
  }
}

int order(int ch) {
  switch (ch) {
  case '*':
  case '/':
    return 3;
  case '+':
  case '-':
    return 2;
  case '(':
    return 1;
  default:
    return -1;
  }
}

void push(int n) {
  if (stkct + 1 > STK_SIZ) {
    puts("Stack overflow.\n");
    exit(1);
  }
  stack[++stkct] = n;
}

int pop(void) {
  if (stkct < 1) {
    puts("Stack underflow.\n");
    exit(1);
  }
  return stack[stkct--];
}
