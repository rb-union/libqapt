<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE policyconfig PUBLIC
 "-//freedesktop//DTD PolicyKit Policy Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/PolicyKit/1.0/policyconfig.dtd">
<policyconfig>
  <vendor>Kubuntu</vendor>
  <vendor_url>http://www.kubuntu.org</vendor_url>

  <action id="@QAPT_WORKER_RDN_VERSIONED@.updatecache">
    <description>Update software sources</description>
    <message>Update software sources</message>
    <defaults>
      <allow_any>no</allow_any>
      <allow_inactive>no</allow_inactive>
      <allow_active>auth_admin_keep</allow_active>
    </defaults>
  </action>
  <action id="@QAPT_WORKER_RDN_VERSIONED@.commitchanges">
    <description>Install or remove packages</description>
    <description xml:lang="zh_CN">安装或移除软件包需要授权</description>
    <message>Install or remove packages</message>
    <message xml:lang="zh_CN">安装或移除软件包需要授权</message>
    <defaults>
      <allow_any>no</allow_any>
      <allow_inactive>no</allow_inactive>
      <allow_active>auth_admin_keep</allow_active>
    </defaults>
  </action>
  <action id="@QAPT_WORKER_RDN_VERSIONED@.writefiletodisk">
    <description>Change system settings</description>
    <message>Change system settings</message>
    <defaults>
      <allow_any>no</allow_any>
      <allow_inactive>no</allow_inactive>
      <allow_active>auth_admin_keep</allow_active>
    </defaults>
  </action>
  <action id="@QAPT_WORKER_RDN_VERSIONED@.cancelforeign">
    <description>Cancel the task of another user</description>
    <message> To cancel someone else's software changes, you need to authenticate.</message>
    <defaults>
      <allow_any>no</allow_any>
      <allow_inactive>no</allow_inactive>
      <allow_active>auth_admin</allow_active>
    </defaults>
  </action>

</policyconfig>
