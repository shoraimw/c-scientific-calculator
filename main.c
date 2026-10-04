#include "raylib.h"
#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_W 470
#define SCREEN_H 820
#define MAX_EXPR 256
#define MAX_TOKENS 256

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_E
#define M_E 2.71828182845904523536
#endif

typedef enum {
    TOK_NUMBER,
    TOK_OP,
    TOK_FUNC,
    TOK_LPAREN,
    TOK_RPAREN
} TokenType;

typedef struct {
    TokenType type;
    double value;
    char text[16];
} Token;

typedef struct {
    Rectangle rect;
    const char *label;
    const char *insert;
    int kind; // 0 normal, 1 function, 2 action, 3 equals, 4 accent
} Button;

static bool degreeMode = true;
static char expression[MAX_EXPR] = "";
static char displayResult[64] = "0";
static char statusMsg[64] = "DEG";
static double ansValue = 0.0;
static bool justEvaluated = false;

static double toRad(double x) { return degreeMode ? x * M_PI / 180.0 : x; }
static double fromRad(double x) { return degreeMode ? x * 180.0 / M_PI : x; }

static int precedence(const char *op) {
    if (strcmp(op, "u-") == 0) return 5;
    if (strcmp(op, "!") == 0) return 6;
    if (strcmp(op, "^") == 0) return 4;
    if (strcmp(op, "*") == 0 || strcmp(op, "/") == 0) return 3;
    if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0) return 2;
    return 0;
}

static bool rightAssociative(const char *op) {
    return strcmp(op, "^") == 0 || strcmp(op, "u-") == 0;
}

static bool isFunctionName(const char *s) {
    const char *f[] = {"sin","cos","tan","asin","acos","atan","sqrt","ln","log","exp","abs","inv"};
    for (size_t i = 0; i < sizeof(f)/sizeof(f[0]); ++i) if (strcmp(s, f[i]) == 0) return true;
    return false;
}

static bool tokenize(const char *src, Token *out, int *count, char *err, size_t errsz) {
    int n = 0;
    int i = 0;
    TokenType prev = TOK_OP;

    while (src[i]) {
        if (isspace((unsigned char)src[i])) { i++; continue; }
        if (n >= MAX_TOKENS) { snprintf(err, errsz, "Too many tokens"); return false; }

        if (isdigit((unsigned char)src[i]) || src[i] == '.') {
            char *endptr;
            double v = strtod(src + i, &endptr);
            if (endptr == src + i) { snprintf(err, errsz, "Bad number"); return false; }
            out[n] = (Token){.type = TOK_NUMBER, .value = v};
            snprintf(out[n].text, sizeof(out[n].text), "%g", v);
            n++;
            i = (int)(endptr - src);
            prev = TOK_NUMBER;
            continue;
        }

        if (isalpha((unsigned char)src[i])) {
            char name[16] = {0};
            int k = 0;
            while (isalpha((unsigned char)src[i]) && k < 15) name[k++] = (char)tolower((unsigned char)src[i++]);
            name[k] = '\0';
            if (strcmp(name, "pi") == 0) {
                out[n] = (Token){.type = TOK_NUMBER, .value = M_PI}; strcpy(out[n].text, "pi"); n++; prev = TOK_NUMBER;
            } else if (strcmp(name, "e") == 0) {
                out[n] = (Token){.type = TOK_NUMBER, .value = M_E}; strcpy(out[n].text, "e"); n++; prev = TOK_NUMBER;
            } else if (strcmp(name, "ans") == 0) {
                out[n] = (Token){.type = TOK_NUMBER, .value = ansValue}; strcpy(out[n].text, "Ans"); n++; prev = TOK_NUMBER;
            } else if (isFunctionName(name)) {
                out[n] = (Token){.type = TOK_FUNC}; strncpy(out[n].text, name, sizeof(out[n].text)-1); n++; prev = TOK_FUNC;
            } else {
                snprintf(err, errsz, "Unknown: %s", name); return false;
            }
            continue;
        }

        if (src[i] == '(') {
            out[n] = (Token){.type = TOK_LPAREN}; strcpy(out[n].text, "("); n++; i++; prev = TOK_LPAREN; continue;
        }
        if (src[i] == ')') {
            out[n] = (Token){.type = TOK_RPAREN}; strcpy(out[n].text, ")"); n++; i++; prev = TOK_RPAREN; continue;
        }

        if (strchr("+-*/^!", src[i])) {
            char op[3] = {src[i], 0, 0};
            if (src[i] == '-' && (prev == TOK_OP || prev == TOK_LPAREN || prev == TOK_FUNC)) strcpy(op, "u-");
            out[n] = (Token){.type = TOK_OP}; strcpy(out[n].text, op); n++; i++; prev = TOK_OP; continue;
        }

        snprintf(err, errsz, "Unexpected '%c'", src[i]);
        return false;
    }
    *count = n;
    return true;
}

static bool toRPN(const Token *in, int count, Token *out, int *outCount, char *err, size_t errsz) {
    Token stack[MAX_TOKENS]; int sp = 0, op = 0;
    for (int i = 0; i < count; ++i) {
        Token t = in[i];
        if (t.type == TOK_NUMBER) out[op++] = t;
        else if (t.type == TOK_FUNC) stack[sp++] = t;
        else if (t.type == TOK_OP) {
            while (sp > 0 && (stack[sp-1].type == TOK_FUNC || stack[sp-1].type == TOK_OP)) {
                Token top = stack[sp-1];
                if (top.type == TOK_FUNC ||
                    precedence(top.text) > precedence(t.text) ||
                    (precedence(top.text) == precedence(t.text) && !rightAssociative(t.text))) {
                    out[op++] = stack[--sp];
                } else break;
            }
            stack[sp++] = t;
        }
        else if (t.type == TOK_LPAREN) stack[sp++] = t;
        else if (t.type == TOK_RPAREN) {
            bool found = false;
            while (sp > 0) {
                if (stack[sp-1].type == TOK_LPAREN) { sp--; found = true; break; }
                out[op++] = stack[--sp];
            }
            if (!found) { snprintf(err, errsz, "Mismatched parentheses"); return false; }
            if (sp > 0 && stack[sp-1].type == TOK_FUNC) out[op++] = stack[--sp];
        }
    }
    while (sp > 0) {
        if (stack[sp-1].type == TOK_LPAREN || stack[sp-1].type == TOK_RPAREN) {
            snprintf(err, errsz, "Mismatched parentheses"); return false;
        }
        out[op++] = stack[--sp];
    }
    *outCount = op;
    return true;
}

static double factorial(double x, bool *ok) {
    if (x < 0 || fabs(x - round(x)) > 1e-10 || x > 170) { *ok = false; return 0; }
    double r = 1.0; for (int i = 2; i <= (int)round(x); ++i) r *= i; return r;
}

static bool evalRPN(const Token *rpn, int count, double *result, char *err, size_t errsz) {
    double st[MAX_TOKENS]; int sp = 0;
    for (int i = 0; i < count; ++i) {
        Token t = rpn[i];
        if (t.type == TOK_NUMBER) st[sp++] = t.value;
        else if (t.type == TOK_OP) {
            if (strcmp(t.text, "u-") == 0) {
                if (sp < 1) { snprintf(err, errsz, "Missing operand"); return false; }
                st[sp-1] = -st[sp-1];
            } else if (strcmp(t.text, "!") == 0) {
                if (sp < 1) { snprintf(err, errsz, "Missing operand"); return false; }
                bool ok = true; st[sp-1] = factorial(st[sp-1], &ok);
                if (!ok) { snprintf(err, errsz, "Factorial domain error"); return false; }
            } else {
                if (sp < 2) { snprintf(err, errsz, "Missing operand"); return false; }
                double b = st[--sp], a = st[--sp], r = 0;
                if (strcmp(t.text, "+") == 0) r = a + b;
                else if (strcmp(t.text, "-") == 0) r = a - b;
                else if (strcmp(t.text, "*") == 0) r = a * b;
                else if (strcmp(t.text, "/") == 0) {
                    if (fabs(b) < 1e-15) { snprintf(err, errsz, "Division by zero"); return false; }
                    r = a / b;
                } else if (strcmp(t.text, "^") == 0) r = pow(a, b);
                if (!isfinite(r)) { snprintf(err, errsz, "Math error"); return false; }
                st[sp++] = r;
            }
        }
        else if (t.type == TOK_FUNC) {
            if (sp < 1) { snprintf(err, errsz, "Missing function argument"); return false; }
            double x = st[--sp], r = 0;
            if (strcmp(t.text, "sin") == 0) r = sin(toRad(x));
            else if (strcmp(t.text, "cos") == 0) r = cos(toRad(x));
            else if (strcmp(t.text, "tan") == 0) r = tan(toRad(x));
            else if (strcmp(t.text, "asin") == 0) r = fromRad(asin(x));
            else if (strcmp(t.text, "acos") == 0) r = fromRad(acos(x));
            else if (strcmp(t.text, "atan") == 0) r = fromRad(atan(x));
            else if (strcmp(t.text, "sqrt") == 0) { if (x < 0) { snprintf(err, errsz, "sqrt domain error"); return false; } r = sqrt(x); }
            else if (strcmp(t.text, "ln") == 0) { if (x <= 0) { snprintf(err, errsz, "ln domain error"); return false; } r = log(x); }
            else if (strcmp(t.text, "log") == 0) { if (x <= 0) { snprintf(err, errsz, "log domain error"); return false; } r = log10(x); }
            else if (strcmp(t.text, "exp") == 0) r = exp(x);
            else if (strcmp(t.text, "abs") == 0) r = fabs(x);
            else if (strcmp(t.text, "inv") == 0) { if (fabs(x) < 1e-15) { snprintf(err, errsz, "Division by zero"); return false; } r = 1.0/x; }
            if (!isfinite(r)) { snprintf(err, errsz, "Math error"); return false; }
            st[sp++] = r;
        }
    }
    if (sp != 1) { snprintf(err, errsz, "Invalid expression"); return false; }
    *result = st[0];
    return true;
}

static bool evaluateExpression(const char *src, double *result, char *err, size_t errsz) {
    Token tok[MAX_TOKENS], rpn[MAX_TOKENS]; int nt = 0, nr = 0;
    if (!tokenize(src, tok, &nt, err, errsz)) return false;
    if (nt == 0) { snprintf(err, errsz, "Enter an expression"); return false; }
    if (!toRPN(tok, nt, rpn, &nr, err, errsz)) return false;
    return evalRPN(rpn, nr, result, err, errsz);
}

static void appendText(const char *s) {
    if (justEvaluated && (isdigit((unsigned char)s[0]) || s[0] == '.' || isalpha((unsigned char)s[0]))) {
        expression[0] = '\0';
        justEvaluated = false;
    } else if (justEvaluated) justEvaluated = false;

    size_t cur = strlen(expression), add = strlen(s);
    if (cur + add < MAX_EXPR - 1) strcat(expression, s);
}

static void backspace(void) {
    size_t n = strlen(expression);
    if (n > 0) expression[n-1] = '\0';
}

static void clearAll(void) {
    expression[0] = '\0';
    strcpy(displayResult, "0");
    snprintf(statusMsg, sizeof(statusMsg), "%s", degreeMode ? "DEG" : "RAD");
    justEvaluated = false;
}

static void doEvaluate(void) {
    char err[64] = {0};
    double v = 0;
    if (evaluateExpression(expression, &v, err, sizeof(err))) {
        ansValue = v;
        if (fabs(v) < 1e-13) v = 0;
        snprintf(displayResult, sizeof(displayResult), "%.12g", v);
        strcpy(statusMsg, "OK");
        justEvaluated = true;
    } else {
        snprintf(displayResult, sizeof(displayResult), "Error");
        snprintf(statusMsg, sizeof(statusMsg), "%s", err);
        justEvaluated = false;
    }
}

static Color withAlpha(Color c, unsigned char a) { c.a = a; return c; }

static void drawTextFit(const char *text, Rectangle r, int maxSize, Color col, int alignRight) {
    int size = maxSize;
    while (size > 12 && MeasureText(text, size) > r.width - 12) size--;
    int x = alignRight ? (int)(r.x + r.width - MeasureText(text, size) - 8) : (int)r.x + 8;
    int y = (int)(r.y + (r.height-size)/2);
    DrawText(text, x, y, size, col);
}

int main(void) {
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_W, SCREEN_H, "C Scientific Calculator");
    SetTargetFPS(60);

    const Color bg = (Color){235,236,239,255};
    const Color body = (Color){37,39,43,255};
    const Color body2 = (Color){24,25,28,255};
    const Color screen = (Color){195,211,194,255};
    const Color key = (Color){73,75,80,255};
    const Color keyDark = (Color){53,55,60,255};
    const Color keyLight = (Color){224,226,230,255};
    const Color yellow = (Color){239,192,72,255};
    const Color red = (Color){210,84,63,255};

    Button buttons[48];
    int bc = 0;
    const int cols = 6;
    const float startX = 45, startY = 335, gap = 7, bw = 58, bh = 48;
    const char *labels[][3] = {
        {"SHIFT","","a"},{"ALPHA","","a"},{"DEG/RAD","","mode"},{"(","(","n"},{")",")","n"},{"DEL","","del"},
        {"sin","sin(","f"},{"cos","cos(","f"},{"tan","tan(","f"},{"sqrt","sqrt(","f"},{"x^y","^","f"},{"AC","","ac"},
        {"asin","asin(","f"},{"acos","acos(","f"},{"atan","atan(","f"},{"ln","ln(","f"},{"log","log(","f"},{"1/x","inv(","f"},
        {"7","7","n"},{"8","8","n"},{"9","9","n"},{"/","/","f"},{"pi","pi","f"},{"e","e","f"},
        {"4","4","n"},{"5","5","n"},{"6","6","n"},{"*","*","f"},{"abs","abs(","f"},{"!","!","f"},
        {"1","1","n"},{"2","2","n"},{"3","3","n"},{"-","-","f"},{"Ans","Ans","f"},{"exp","exp(","f"},
        {"0","0","n"},{".",".","n"},{"+/-","-","f"},{"+","+","f"},{"=","","eq"},{"=","","eq"}
    };

    for (int r = 0; r < 7; ++r) {
        for (int c = 0; c < cols; ++c) {
            if (r == 6 && c == 5) continue; // last '=' merged visually
            float x = startX + c*(bw+gap);
            float y = startY + r*(bh+gap);
            float w = bw;
            if (r == 6 && c == 4) w = bw*2 + gap;
            Button b = {.rect={x,y,w,bh}, .label=labels[r*6+c][0], .insert=labels[r*6+c][1], .kind=0};
            const char *t = labels[r*6+c][2];
            if (strcmp(t,"f")==0) b.kind=1; else if(strcmp(t,"eq")==0) b.kind=3; else if(strcmp(t,"a")==0 || strcmp(t,"mode")==0 || strcmp(t,"del")==0 || strcmp(t,"ac")==0) b.kind=2;
            buttons[bc++] = b;
        }
    }

    while (!WindowShouldClose()) {
        Vector2 m = GetMousePosition();
        bool click = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        int ch = GetCharPressed();
        while (ch > 0) {
            if ((ch >= '0' && ch <= '9') || strchr(".+-*/^()", ch)) {
                char s[2]={(char)ch,0}; appendText(s);
            }
            ch = GetCharPressed();
        }
        if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) doEvaluate();
        if (IsKeyPressed(KEY_BACKSPACE)) backspace();
        if (IsKeyPressed(KEY_ESCAPE)) clearAll();

        for (int i = 0; i < bc; ++i) {
            Button *b = &buttons[i];
            if (click && CheckCollisionPointRec(m, b->rect)) {
                if (strcmp(b->label,"AC")==0) clearAll();
                else if (strcmp(b->label,"DEL")==0) backspace();
                else if (strcmp(b->label,"DEG/RAD")==0) { degreeMode=!degreeMode; snprintf(statusMsg,sizeof(statusMsg),"%s",degreeMode?"DEG":"RAD"); }
                else if (strcmp(b->label,"SHIFT")==0) snprintf(statusMsg,sizeof(statusMsg),"SHIFT: use asin/acos/atan keys");
                else if (strcmp(b->label,"ALPHA")==0) snprintf(statusMsg,sizeof(statusMsg),"ALPHA: pi, e, Ans available");
                else if (b->kind==3) doEvaluate();
                else appendText(b->insert);
            }
        }

        BeginDrawing();
        ClearBackground(bg);

        DrawRectangleRounded((Rectangle){25,18,420,784}, 0.08f, 18, body2);
        DrawRectangleRounded((Rectangle){31,24,408,772}, 0.07f, 18, body);
        DrawRectangleGradientV(50, 42, 370, 74, (Color){65,67,72,255}, (Color){28,29,32,255});
        DrawText("C-SCI", 58, 50, 28, RAYWHITE);
        DrawText("991 PRO", 58, 81, 16, (Color){204,207,211,255});
        DrawText("NATURAL-V.P.A.M.", 58, 100, 9, (Color){169,172,177,255});
        DrawRectangleRounded((Rectangle){290,50,112,45}, 0.08f, 10, (Color){42,30,28,255});
        DrawRectangleRounded((Rectangle){296,56,100,33}, 0.06f, 8, (Color){81,49,38,255});
        DrawText("2nd edition", 339, 104, 9, (Color){177,180,184,255});

        Rectangle lcd = {52,130,366,132};
        DrawRectangleRounded(lcd, 0.04f, 8, (Color){17,18,20,255});
        DrawRectangleRounded((Rectangle){58,136,354,120}, 0.02f, 6, screen);
        DrawText(degreeMode?"DEG":"RAD", 66, 142, 14, (Color){45,56,48,255});
        DrawText("Math", 356, 142, 13, (Color){45,56,48,255});
        Rectangle exprRect = {66,163,338,42};
        drawTextFit(expression[0]?expression:"0", exprRect, 25, (Color){31,42,34,255}, 0);
        Rectangle resRect = {66,204,338,38};
        drawTextFit(displayResult, resRect, 31, (Color){20,31,24,255}, 1);

        DrawText(statusMsg, 58, 271, 12, (Color){233,194,69,255});
        DrawText("SCIENTIFIC CALCULATOR • PURE C + RAYLIB", 58, 292, 11, (Color){173,176,181,255});

        for (int i = 0; i < bc; ++i) {
            Button *b = &buttons[i];
            bool hover = CheckCollisionPointRec(m, b->rect);
            Color c = key;
            Color txt = RAYWHITE;
            if (b->kind==1) c = keyDark;
            if (strcmp(b->label,"AC")==0) c = (Color){84,120,52,255};
            if (strcmp(b->label,"DEL")==0) c = (Color){93,135,57,255};
            if (b->kind==3) c = (Color){81,98,145,255};
            if (b->kind==2 && (strcmp(b->label,"SHIFT")==0 || strcmp(b->label,"ALPHA")==0)) {
                c = (Color){47,49,54,255}; txt = strcmp(b->label,"SHIFT")==0 ? yellow : red;
            }
            if (hover) c = ColorBrightness(c, 0.12f);
            DrawRectangleRounded(b->rect, 0.18f, 8, c);
            DrawRectangleRoundedLinesEx(b->rect, 0.18f, 8, 1.0f, withAlpha(BLACK, 90));
            int fs = (strlen(b->label) > 5) ? 13 : 20;
            int tx = (int)(b->rect.x + (b->rect.width - MeasureText(b->label, fs))/2);
            int ty = (int)(b->rect.y + (b->rect.height - fs)/2 - 1);
            DrawText(b->label, tx, ty, fs, txt);
        }

        DrawText("Keyboard: 0-9  + - * / ^ ( )  Enter=  Backspace=DEL  Esc=AC", 38, 781, 9, (Color){180,183,187,255});
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
