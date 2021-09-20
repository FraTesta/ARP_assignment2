fileName = fopen('token_rf_10.txt','r');
token = fscanf(fileName,'%f');
fclose(fileName);

fileName = fopen('token_rf10_sec.txt','r');
time = fscanf(fileName,'%f');
fclose(fileName);

size(token)
size(time)

t = [1:1:length(token)];
figure(1);
plot(time,token);
xlabel('time ');
ylabel('token value');