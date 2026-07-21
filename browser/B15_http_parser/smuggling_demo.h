#ifndef SMUGGLING_DEMO_H
#define SMUGGLING_DEMO_H

/* CL.TE attack: front-end trusts Content-Length, backend trusts TE:chunked */
void demo_cl_te(void);

/* TE.CL attack: front-end trusts TE:chunked, backend trusts Content-Length */
void demo_te_cl(void);

/* Print summary of smuggling mitigations */
void smuggling_summary(void);

#endif /* SMUGGLING_DEMO_H */
