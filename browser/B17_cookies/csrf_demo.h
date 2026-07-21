#ifndef CSRF_DEMO_H
#define CSRF_DEMO_H

/* Demonstrate CSRF attack using SameSite=None cookie */
void csrf_demo_attack(void);

/* Show how SameSite=Strict prevents the attack */
void csrf_demo_fix(void);

/* Show HttpOnly protection against XSS cookie theft */
void csrf_demo_httponly(void);

#endif /* CSRF_DEMO_H */
