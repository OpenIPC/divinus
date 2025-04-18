SRC = $(shell find ./ -name '*.c')
OBJ = ../divinus

.PHONY: clean divinus
divinus: $(OBJ)

$(OBJ): $(SRC:%.c=%.o)
	$(CC) $^ -rdynamic $(OPT) -L../lib -lmi_vdf -lmi_ive -lcus3a -lMD_LINUX -lcam_fs_wrapper -lmi_divp -lmi_ipu -lmi_rgn -lmi_venc -lmi_isp -lmi_sensor -lispalgo -lmi_sys -lmi_ao -lmi_vif -lmi_vpe -lmi_ai -lmi_shadow -lcam_os_wrapper -o $@

%.o: %.c Makefile
	$(CC) -c $< $(OPT) -o $@

clean:
	rm -rf $(SRC:%.c=%.o) $(OBJ)
