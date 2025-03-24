%% DAQ Binary Data Reader
clear;
clc;
close all;

%% DAQ HW Configuration Definitions from main.cpp
TARGET_ADC_FREQ = 5000; % Define Target ADC Sampling Frequency (Hz)
TARGET_SD_WRITE_FREQ = 10; % Define SD Card Write Frequency (Hz)
BUF_END_PAD = 0; % Define DMA Buffer End Extra Padding (Unit is HWORD - 16 Bits)
ADC_PARALLEL_CHANNELS = 8; % Define Number of Concurrent ADC Channels

% Define Final ADC-DMA Buffer Length
ADC_DMA_BUFLEN = ADC_PARALLEL_CHANNELS * (TARGET_ADC_FREQ / TARGET_SD_WRITE_FREQ + BUF_END_PAD);
% Define ADC Sample Resolution
ADC_SAMPLE_RES = 12;

%% Define Data Storage Structure with DMA Buffer from main.cpp
##struct DataBuf
##{
##  uint32_t timestamp;                 // Stores Time of SD Write From Controller Power Up
##  int16_t DMA_ADCBuf[ADC_DMA_BUFLEN]; // Define Buffer for DMA Transfer of ADC Results
##  uint32_t bufstat;                   // Define Flag Container to Track Buffer Status
##} Buf1, Buf2;                         // Define 1st Buffer for ADC-DMA Writes and 2nd Buffer for SD Card Writes

%% Tested With Combinations of VCC, GND and analogWrite(A0, 343)

%% Open DAT file
dat = fopen("Test6.DAT");

%% Read Time Data
% Read DAQ Initial Log Timestamp
t_start = fread(dat, 1, "uint32");

% Read All Data Buffer Timestamps
fseek(dat, 4); % Skip Initial Timestamp
raw_time = fread(dat, Inf, "uint32", 2 * ADC_DMA_BUFLEN + 4); % Skip DMA_ADCBuf Array and bufstat Bytes in DataBuf
% Subtract Start Time from Buffer Timestamps
raw_time -= t_start;

% Reconstruct Sample Times for First Data Buffer
% Note: This is Possible as ADC Sampling was Observed to be Consistent in Time
time = 0:raw_time(1) / ADC_DMA_BUFLEN:raw_time(1);
time = time(1, 1: (end - 1)); % Discard Last Time Value as it is the Time Stamp of the Next Data Buffer

% Reconstruct Sample Time for Remaining Data Buffers
% Note: This is Possible as ADC Sampling was Observed to be Consistent in Time
for i = 1:(size(raw_time, 1) - 1)
  % Reconstruct and Append Sample Times
  time = [time raw_time(i):(raw_time(i + 1) - raw_time(i)) / ADC_DMA_BUFLEN:raw_time(i + 1)];
  time = time(1, 1: (end - 1));
endfor

% Convert Time Vector to Column Vector
time = time';

% Deinterleave ADC Channel Sample Times from Serial Sample Time Vector
for i=1:ADC_PARALLEL_CHANNELS
  ADC_CH_time(i, :) = time(i:ADC_PARALLEL_CHANNELS:end);
endfor

%% Read ADC Channel Data
% Read ADC Data from 1st Data Buffer
fseek(dat, 4 + 4); % Skip Initial Timestamp and First Data Buffer's Timestamp
raw_val = fread(dat, ADC_DMA_BUFLEN, "int16");

% Read ADC Data from Subsequent Data Buffers
for i = 1:(size(raw_time, 1) - 1)
  % Skip Initial Timestamp and 1st Data Buffer's Timestamp
  % Increment File Pointer in Multiples of Data Buffer Size in Bytes
  fseek(dat, 4 + 4 + i * (4 + 2 * ADC_DMA_BUFLEN + 4));
  raw_val_tmp = fread(dat, ADC_DMA_BUFLEN, "int16"); % Read ADC Data from Selected Data Buffer
  raw_val = [raw_val; raw_val_tmp]; % Append Selected Data Buffer's ADC Data
endfor

% Convert ADC Serial Data to Voltage
raw_val *= 3.3 / (pow2(ADC_SAMPLE_RES) - 1);

% Deinterleave ADC Channel Data from ADC Serial Data Vector
for i=1:ADC_PARALLEL_CHANNELS
  ADC_CH_val(i, :) = raw_val(i:ADC_PARALLEL_CHANNELS:end);
endfor

%% Drop ADC Channel and Sample Data for ADC Input Channels 8 and 9
% Drop ADC Channel Data
ADC_CH_val(5, :) = ADC_CH_val(7, :);
ADC_CH_val(6, :) = ADC_CH_val(8, :);
ADC_CH_val = ADC_CH_val(1:6, :);

% Drop Sample Time Data
ADC_CH_time(5, :) = ADC_CH_time(7, :);
ADC_CH_time(6, :) = ADC_CH_time(8, :);
ADC_CH_time = ADC_CH_time(1:6, :);

%% Clear Unnecessary Variables to Save Memory
clear -x ADC_CH_time ADC_CH_val;

%% Plot ADC Channel 10 - Pin A1 Data
figure(1, 'name', 'ADC Channel 10 - Pin A1 - Voltage Trace');
plot(ADC_CH_time(5, :), ADC_CH_val(5, :));
grid on;
grid minor on;
title('Pin A1 - Voltage Trace');
axis('tight');
ylabel('Voltage (V)');
xlabel('Time (usec)');
set(gca, 'fontsize', 18);

%% Plot ADC Channel 11 - Pin A2 Data
figure(2, 'name', 'ADC Channel 11 - Pin A2 - Voltage Trace');
plot(ADC_CH_time(6, :), ADC_CH_val(6, :));
grid on;
grid minor on;
title('Pin A2 - Voltage Trace');
axis('tight');
ylabel('Voltage (V)');
xlabel('Time (usec)');
set(gca, 'fontsize', 18);

%% Plot ADC Channel 4 - Pin A3 Data
figure(3, 'name', 'ADC Channel 4 - Pin A3 - Voltage Trace');
plot(ADC_CH_time(1, :), ADC_CH_val(1, :));
grid on;
grid minor on;
title('Pin A3 - Voltage Trace');
axis('tight');
ylabel('Voltage (V)');
xlabel('Time (usec)');
set(gca, 'fontsize', 18);

%% Plot ADC Channel 5 - Pin A4 Data
figure(4, 'name', 'ADC Channel 5 - Pin A4 - Voltage Trace');
plot(ADC_CH_time(2, :), ADC_CH_val(2, :));
grid on;
grid minor on;
title('Pin A4 - Voltage Trace');
axis('tight');
ylabel('Voltage (V)');
xlabel('Time (usec)');
set(gca, 'fontsize', 18);

%% Plot ADC Channel 6 - Pin A5 Data
figure(5, 'name', 'ADC Channel 6 - Pin A5 - Voltage Trace');
plot(ADC_CH_time(3, :), ADC_CH_val(3, :));
grid on;
grid minor on;
title('Pin A5 - Voltage Trace');
axis('tight');
ylabel('Voltage (V)');
xlabel('Time (usec)');
set(gca, 'fontsize', 18);

%% Plot ADC Channel 7 - Pin A6 Data
figure(6, 'name', 'ADC Channel 7 - Pin A6 - Voltage Trace');
plot(ADC_CH_time(4, :), ADC_CH_val(4, :));
grid on;
grid minor on;
title('Pin A6 - Voltage Trace');
axis('tight');
ylabel('Voltage (V)');
xlabel('Time (usec)');
set(gca, 'fontsize', 18);

%% close DAT file
fclose("all");
