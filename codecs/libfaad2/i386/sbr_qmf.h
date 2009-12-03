static void RENAME(sbr_qmf_analysis_32)(sbr_info *sbr, qmfa_info *qmfa, const real_t *input,
                         qmf_t X[MAX_NTSRHFG][32], uint8_t offset, uint8_t kx)
{
    ALIGN real_t u[64];
#ifndef SBR_LOW_POWER
    ALIGN real_t x[64], y[64];
#else
    ALIGN real_t y[32];
#endif
    uint16_t in = 0;
    uint8_t l;

    /* qmf subsample l */
    for (l = 0; l < sbr->numTimeSlotsRate; l++)
    {
        int16_t n;

        /* shift input buffer x */
        memmove(qmfa->x + 32, qmfa->x, (320-32)*sizeof(real_t));

        /* add new samples to input buffer x */
        for (n = 32 - 1; n >= 0; n--)
        {
#ifdef FIXED_POINT
            qmfa->x[n] = (input[in++]) >> 5;
#else
            qmfa->x[n] = input[in++];
#endif
        }

        /* window and summation to create array u */
        for (n = 0; n < 64; n++)
        {
            u[n] = MUL_F(qmfa->x[n], qmf_c[2*n]) +
                MUL_F(qmfa->x[n + 64], qmf_c[2*(n + 64)]) +
                MUL_F(qmfa->x[n + 128], qmf_c[2*(n + 128)]) +
                MUL_F(qmfa->x[n + 192], qmf_c[2*(n + 192)]) +
                MUL_F(qmfa->x[n + 256], qmf_c[2*(n + 256)]);
        }

        /* calculate 32 subband samples by introducing X */
#ifdef SBR_LOW_POWER
        y[0] = u[48];
        for (n = 1; n < 16; n++)
            y[n] = u[n+48] + u[48-n];
        for (n = 16; n < 32; n++)
            y[n] = -u[n-16] + u[48-n];

        DCT3_32_unscaled(u, y);

        for (n = 0; n < 32; n++)
        {
            if (n < kx)
            {
#ifdef FIXED_POINT
                QMF_RE(X[l + offset][n]) = u[n] << 1;
#else
                QMF_RE(X[l + offset][n]) = 2. * u[n];
#endif
            } else {
                QMF_RE(X[l + offset][n]) = 0;
            }
        }
#else
        x[0] = u[0];
        for (n = 0; n < 31; n++)
        {
            x[2*n+1] = u[n+1] + u[63-n];
            x[2*n+2] = u[n+1] - u[63-n];
        }
        x[63] = u[32];

        DCT4_64_kernel(y, x);

        for (n = 0; n < 32; n++)
        {
            if (n < kx)
            {
#ifdef FIXED_POINT
                QMF_RE(X[l + offset][n]) = y[n] << 1;
                QMF_IM(X[l + offset][n]) = -y[63-n] << 1;
#else
                QMF_RE(X[l + offset][n]) = 2. * y[n];
                QMF_IM(X[l + offset][n]) = -2. * y[63-n];
#endif
            } else {
                QMF_RE(X[l + offset][n]) = 0;
                QMF_IM(X[l + offset][n]) = 0;
            }
        }
#endif
    }
}

#ifdef SBR_LOW_POWER
static void RENAME(sbr_qmf_synthesis_64)(sbr_info *sbr, qmfs_info *qmfs, qmf_t X[MAX_NTSRHFG][64],
                          real_t *output)
{
    ALIGN real_t x[64];
    ALIGN real_t y[64];
    int16_t n, k, out = 0;
    uint8_t l;


    /* qmf subsample l */
    for (l = 0; l < sbr->numTimeSlotsRate; l++)
    {
        //real_t *v0, *v1;

        /* shift buffers */
        //memmove(qmfs->v[0] + 64, qmfs->v[0], (640-64)*sizeof(real_t));
        //memmove(qmfs->v[1] + 64, qmfs->v[1], (640-64)*sizeof(real_t));
        memmove(qmfs->v[0] + 128, qmfs->v[0], (1280-128)*sizeof(real_t));

        //v0 = qmfs->v[qmfs->v_index];
        //v1 = qmfs->v[(qmfs->v_index + 1) & 0x1];
        //qmfs->v_index = (qmfs->v_index + 1) & 0x1;

        /* calculate 128 samples */
        for (k = 0; k < 64; k++)
        {
#ifdef FIXED_POINT
            x[k] = QMF_RE(X[l][k]);
#else
            x[k] = QMF_RE(X[l][k]) / 32.;
#endif
        }

        for (n = 0; n < 32; n++)
        {
            y[2*n]   = -x[2*n];
            y[2*n+1] =  x[2*n+1];
        }

        DCT2_64_unscaled(x, x);

        for (n = 0; n < 64; n++)
        {
            qmfs->v[0][n+32] = x[n];
        }
        for (n = 0; n < 32; n++)
        {
            qmfs->v[0][31 - n] = x[n + 1];
        }
        DST2_64_unscaled(x, y);
        qmfs->v[0][96] = 0;
        for (n = 1; n < 32; n++)
        {
            qmfs->v[0][n + 96] = x[n-1];
        }

        /* calculate 64 output samples and window */
        for (k = 0; k < 64; k++)
        {
#if 1
             output[out++] = MUL_F(qmfs->v[0][k], qmf_c[k]) +
                 MUL_F(qmfs->v[0][192 + k], qmf_c[64 + k]) +
                 MUL_F(qmfs->v[0][256 + k], qmf_c[128 + k]) +
                 MUL_F(qmfs->v[0][256 + 192 + k], qmf_c[128 + 64 + k]) +
                 MUL_F(qmfs->v[0][512 + k], qmf_c[256 + k]) +
                 MUL_F(qmfs->v[0][512 + 192 + k], qmf_c[256 + 64 + k]) +
                 MUL_F(qmfs->v[0][768 + k], qmf_c[384 + k]) +
                 MUL_F(qmfs->v[0][768 + 192 + k], qmf_c[384 + 64 + k]) +
                 MUL_F(qmfs->v[0][1024 + k], qmf_c[512 + k]) +
                 MUL_F(qmfs->v[0][1024 + 192 + k], qmf_c[512 + 64 + k]);
#else
            output[out++] = MUL_F(v0[k], qmf_c[k]) +
                MUL_F(v0[64 + k], qmf_c[64 + k]) +
                MUL_F(v0[128 + k], qmf_c[128 + k]) +
                MUL_F(v0[192 + k], qmf_c[192 + k]) +
                MUL_F(v0[256 + k], qmf_c[256 + k]) +
                MUL_F(v0[320 + k], qmf_c[320 + k]) +
                MUL_F(v0[384 + k], qmf_c[384 + k]) +
                MUL_F(v0[448 + k], qmf_c[448 + k]) +
                MUL_F(v0[512 + k], qmf_c[512 + k]) +
                MUL_F(v0[576 + k], qmf_c[576 + k]);
#endif
        }
    }
}
#else
static void RENAME(sbr_qmf_synthesis_64)(sbr_info *sbr, qmfs_info *qmfs, qmf_t X[MAX_NTSRHFG][64],
                          real_t *output)
{
    ALIGN real_t x1[64], x2[64];
    real_t scale = 1.f/64.f;
    int16_t n, k, out = 0;
    uint8_t l;


    /* qmf subsample l */
    for (l = 0; l < sbr->numTimeSlotsRate; l++)
    {
        real_t *v0, *v1;

        /* shift buffers */
        memmove(qmfs->v[0] + 64, qmfs->v[0], (640-64)*sizeof(real_t));
        memmove(qmfs->v[1] + 64, qmfs->v[1], (640-64)*sizeof(real_t));

        v0 = qmfs->v[qmfs->v_index];
        v1 = qmfs->v[(qmfs->v_index + 1) & 0x1];
        qmfs->v_index = (qmfs->v_index + 1) & 0x1;

        /* calculate 128 samples */
        x1[0] = scale*QMF_RE(X[l][0]);
        x2[63] = scale*QMF_IM(X[l][0]);
        for (k = 0; k < 31; k++)
        {
            x1[2*k+1] = scale*(QMF_RE(X[l][2*k+1]) - QMF_RE(X[l][2*k+2]));
            x1[2*k+2] = scale*(QMF_RE(X[l][2*k+1]) + QMF_RE(X[l][2*k+2]));

            x2[61 - 2*k] = scale*(QMF_IM(X[l][2*k+2]) - QMF_IM(X[l][2*k+1]));
            x2[62 - 2*k] = scale*(QMF_IM(X[l][2*k+2]) + QMF_IM(X[l][2*k+1]));
        }
        x1[63] = scale*QMF_RE(X[l][63]);
        x2[0] = scale*QMF_IM(X[l][63]);

        DCT4_64_kernel(x1, x1);
        DCT4_64_kernel(x2, x2);

        for (n = 0; n < 32; n++)
        {
            v0[   2*n]   =  x2[2*n]   - x1[2*n];
            v1[63-2*n]   =  x2[2*n]   + x1[2*n];
            v0[   2*n+1] = -x2[2*n+1] - x1[2*n+1];
            v1[62-2*n]   = -x2[2*n+1] + x1[2*n+1];
        }

        /* calculate 64 output samples and window */
        for (k = 0; k < 64; k++)
        {
            output[out++] = MUL_F(v0[k], qmf_c[k]) +
                MUL_F(v0[64 + k], qmf_c[64 + k]) +
                MUL_F(v0[128 + k], qmf_c[128 + k]) +
                MUL_F(v0[192 + k], qmf_c[192 + k]) +
                MUL_F(v0[256 + k], qmf_c[256 + k]) +
                MUL_F(v0[320 + k], qmf_c[320 + k]) +
                MUL_F(v0[384 + k], qmf_c[384 + k]) +
                MUL_F(v0[448 + k], qmf_c[448 + k]) +
                MUL_F(v0[512 + k], qmf_c[512 + k]) +
                MUL_F(v0[576 + k], qmf_c[576 + k]);
        }
    }
}
#endif
