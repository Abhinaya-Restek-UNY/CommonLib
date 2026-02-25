idf_component_register(SRCS "I2CDevice.cpp"
                    INCLUDE_DIRS "include"
		    REQUIRES esp_driver_i2c esp_timer
	    )
