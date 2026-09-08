/* ===================================================================
 * ARCHIVO: model_mahalanobis.h
 * PROYECTO: IgnisEdge
 * DESCRIPCIÓN: Parámetros estáticos exportados desde Python
 * =================================================================== */

// 1. Parámetros del StandardScaler (Media y Desviación Estándar)
const float scaler_mean[5] = {175145.825591, 0.000960, -0.003926, 0.109077, 675996.374955};
const float scaler_std[5]  = {39444.929155, 0.021481, 0.069373, 2.228910, 3797319.456456};

// 2. Vector de Medias del modelo Mahalanobis (mu)
const float mu_mahal[5] = {0.00000000, 0.00000000, -0.00000000, 0.00000000, 0.00000000};

// 3. Matriz de Covarianza Inversa (5x5)
const float inv_cov_matrix[5][5] = {
    {1.029842, 0.150801, 0.168383, 0.118290, -0.027166},
    {0.150801, 1.408484, 0.763899, 0.273034, 0.024199},
    {0.168383, 0.763899, 1.468874, 0.360645, 0.102129},
    {0.118290, 0.273034, 0.360645, 1.101538, 0.046126},
    {-0.027166, 0.024199, 0.102129, 0.046126, 1.009964},
};

// 4. Umbral de Detección (99.5% Confianza)
const float UMBRAL_MAHALANOBIS = 7.7440;