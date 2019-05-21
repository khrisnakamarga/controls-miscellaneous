%ME 471 - Lab
%Khrisna Kamarga
clear all; close all; clc;
cd 'C:\Users\Khrisna Adi Kamarga\Desktop\Senior Fall\ME 471'
load("myRio.mat");

%Constants
Ka = 0.41; %A/V
Km = 0.11; %Nm/A
B = 0.001; %Nms/rad
J = 0.00034; %Nms^2/rad (J = Jm + JL)
imax = 4.1; %A
theta0 = pi; %rad
PM_rec = 59; %degrees

%System
taum = J/B;
Kda = Ka*Km/B;
s = tf('s');
Tm = Kda/(s*(taum*s + 1));
margin(Tm)

%Proportional Compensator (make the phase margin 59)
Kp = 0.0458;
margin(Kp*Tm);
[Gm Pm] = margin(Tm);


% Lead Compensator
phi_max = PM_rec - Pm + 5.5;
%Finding beta
syms beta
beta = solve(sind(phi_max) == (1 - beta)/(1 + beta), beta);
beta = vpa(beta);
beta = double(beta);
%Finding K
K = imax*beta/(Ka*theta0);

%finding wmax
w = 12:0.005:14;
[M, P, wout] =  bode(Tm, w);
wmax = interp1(squeeze(M), wout, sqrt(beta)/K, 'linear');
%finding T
syms T
T = solve(wmax == 1/(T*sqrt(beta)), T);
T = vpa(T);
T = double(T);



%Compensator
% comp = zpk([-1/T], [-1/(beta*T)], K/beta)
comp = zpk([-z_e], [-p_e], Kph_e);

%System Simulation
L = series(comp, Tm);
figure
margin(L);
figure
pzmap(feedback(L, 1));
figure
step(feedback(L, 1));

% Lab Report
close all; clc;
figure
bode(Tm);
hold on
bode(Kp*Tm);
hold on
bode(comp*Tm);
legend("Uncompensated System", "Proportional", "Proportional + Phase Lead");
hold off

figure
margin(comp*Tm);

figure
step(feedback(Kp*Tm, 1));
hold on
step(feedback(comp*Tm, 1));
hold on
step(feedback(Tm, 1));
legend("P", "PD", "Uncompensated");
title("\theta Step Response");
hold off

figure
step(feedback(Kp, Tm));
hold on
step(feedback(comp, Tm));
legend("P", "PD");
title("V_{da} Step Response");
axis([0 2 -1 1.5]);

%% Phase Lead Compensator
t = linspace(0, dt*length(theta_e), length(theta_e));
figure
step(mean(thetaRef_e)*feedback(comp*Tm, 1));
hold on
plot(t, theta_e, "r--");
legend("theoretical","experimental","Location","best");
xlabel("time (s)");
ylabel("angular position (rad)");
title("Theoretical vs. Experimental Result");
axis([0 1 0 3.5]);

%% Shifted up
figure
step(mean(thetaRef_e)*feedback(comp*Tm, 1));
hold on
plot(t, theta_e + (thetaRef_e - theta_e(end)), "r--");
legend("theoretical","experimental","Location","best");
xlabel("time (s)");
ylabel("angular position (rad)");
title("Anglular Position, Theoretical vs. Experimental Result");
axis([0 1 0 4]);

%% Voltage

figure
step((1/0.45)*abs(min(Vc_e))*feedback(comp, Tm));
hold on
title("Voltage, Theorertical vs. Experimental");
xlabel("time (s)");
ylabel("V_{da} (V)");
plot(t, Vc_e, "r--");
legend("theoretical","experimental","Location","best");
