library(ggplot2)

# Create a data frame with your data
# Read data from text file
data <- read.table("DATA.txt", header = TRUE, sep = ";", row.names = NULL)

# Rename columns to match the original data frame
colnames(data) <- c("NTU","Number of meas", "V", "Temperature")

# View the first few rows of the data
head(data)
# Fit a linear regression model
model <- lm(NTU ~ V, data = data)

# Retrieve the coefficients
a <- coef(model)[2]  # Slope (correlation coefficient)
b <- coef(model)[1]  # Intercept
# Extract residuals
residuals <- resid(model)

# Compute standard deviation of residuals
residual_sd <- sd(residuals)
# Create the correlation equation
correlation_equation <- function(Voltage) {
  NTU <- a * Voltage + b
  return(NTU)
}

# Use the correlation equation to convert voltage measurements to NTU
voltage_measurement <- 2.0  # Replace with your actual voltage measurement
ntu_measurement <- correlation_equation(voltage_measurement)
cat("Voltage:", voltage_measurement, "V -> NTU:", ntu_measurement, "NTU\n")

summary_data <- summary(model)
r_squared <- summary_data$r.squared

# Create a ggplot with data points and the correlation equation line
p <- ggplot(data, aes(x = V, y = NTU)) +
  geom_point(size = 2) +  # Increase point size to 2
  geom_abline(intercept = b, slope = a, color = "indianred2", size = 1.5) +  # Increase line size to 1.5
  labs(x = "Voltage (V)", y = "NTU") +  # Axis labels
  ggtitle(paste("Calibration curve")) +  # Title with R²
  geom_text(aes(label = paste("NTU = ", round(a, 2), " * V ", round(b, 2), sep="")), 
            x = min(data$V), y = max(data$NTU), hjust = -1, vjust = 3, color = "indianred2",
            size = 6) +  # Increase text size to 16
  geom_text(aes(label = paste("R² =", round(r_squared, 4))), x = min(data$V), y = max(data$NTU), 
            hjust =-3, vjust = 5, color = "slateblue3", size = 6) +  # Increase text size to 16
  theme(
    text = element_text(size = 20),  # Increase default text size to 16
    axis.text = element_text(size = 20),  # Increase axis text size to 16
    axis.title = element_text(size = 20)  # Increase axis title size to 16
  )

# Print the plot
print(p)

cat("Voltage:", voltage_measurement, "V -> NTU:", ntu_measurement, "NTU\n")
cat("R-squared:", r_squared, "\n")
cat("Residual standard deviation:", residual_sd, "\n")
