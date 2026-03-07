idf_component_register(SRCS "UDPHandler.cpp"
                    INCLUDE_DIRS "include"
		PRIV_REQUIRES mbedtls esp_netif
	    )
