clear; clc; close all;

data = importdata('log.txt');

current = data(:, 1) / 1000;
current_target = data(:, 2) / 1000;
duty = data(:, 3) / (2^16 - 1);
voltage = data(:, 4) / 1000;
hall = data(:, 5);
throttle = data(:, 6);

figure;
ax1 = subplot(3, 1, 1);
plot(current, '.-');

ax2 = subplot(3, 1, 2);
plot(hall, '.-');

ax3 = subplot(3, 1, 3);
plot(duty, '.-');

linkaxes([ax1, ax2, ax3],'x');