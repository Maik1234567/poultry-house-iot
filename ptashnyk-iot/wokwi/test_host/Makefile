# Перевірка скетча Wokwi на ПК без ESP32 (заглушки бібліотек Arduino).
test:
	g++ -std=c++17 -DCORE3 -I. -I.. -x c++ main.cpp ../climate_controller.cpp -o t3 && ./t3 | grep -v '^{'
	g++ -std=c++17 -DCORE2 -I. -I.. -x c++ main.cpp ../climate_controller.cpp -o t2 && ./t2 | grep -v '^{'
clean:
	rm -f t2 t3
