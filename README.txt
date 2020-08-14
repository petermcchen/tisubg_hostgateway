2020.08.13
Add subgcli.c file from Adam who sent on 2020.07.21.
2020.07.14
Implement data parser when DEVICE_DATA_RX_IND.
Use corrent number for LLC commands.
2020.07.06
Maintain gateway client code version control for easy maintenance.
This client code (socketread.c, socketwrite.c, etc) are used with tisubg_hostcollector which need TI-15.4 Stack CoP.
The CMD1 and Sub-command ID may changed for system design change.
